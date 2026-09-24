#include "Gemini.hpp"

#include "Translator.hpp"

#include <Geode/utils/StringMap.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/web.hpp>

#include <cctype>
#include <chrono>
#include <fmt/format.h>
#include <set>

using namespace geode::prelude;

namespace {
    // 한 번 켜 둔 동안 물어보는 횟수의 상한. 표에 없는 문장이 쏟아져도 요청이
    // 끝없이 나가지는 않게 한다.
    constexpr int SESSION_LIMIT = 300;
    constexpr int MAX_IN_FLIGHT = 3;

    // 고유명사를 건드리지 말라고 못박아 둔다. 레벨 제목은 만든 사람이 붙인
    // 이름이지 번역할 문구가 아니다.
    constexpr std::string_view PROMPT =
        "Translate this Geometry Dash interface text into Korean.\n"
        "Reply with the translation only: no quotes, no notes, no alternatives.\n"
        "Keep it short enough to fit on a button.\n"
        "If the text is a proper noun such as a level title, a song title or a "
        "player name, reply with it unchanged.\n\n"
        "Text: ";

    bool g_enabled = false;
    std::string g_key;
    std::string g_model;

    geode::utils::StringMap<std::string> g_learned;
    std::set<std::string, std::less<>> g_pending;  // 지금 물어보는 중
    std::set<std::string, std::less<>> g_refused;  // 실패했거나 바꾸지 않기로 한 것
    int g_asked = 0;
    int g_inFlight = 0;

    std::filesystem::path cachePath() {
        return Mod::get()->getConfigDir() / "learned.json";
    }

    void save() {
        auto object = matjson::Value::object();
        for (auto const& [source, korean] : g_learned) {
            object.set(source, matjson::Value(korean));
        }
        if (file::writeString(cachePath(), object.dump()).isErr()) {
            log::warn("could not write the machine translation file");
        }
    }

    std::string tidy(std::string text) {
        // 모델이 앞뒤로 줄바꿈이나 따옴표를 붙여 보내는 일이 있다.
        auto const spaces = " \t\r\n\"'";
        auto const first = text.find_first_not_of(spaces);
        if (first == std::string::npos) {
            return {};
        }
        return text.substr(first, text.find_last_not_of(spaces) - first + 1);
    }
}

namespace kopatch::gemini {
    void configure(bool on, std::string key, std::string model) {
        g_enabled = on;
        g_key = std::move(key);
        g_model = std::move(model);
    }

    bool enabled() {
        return g_enabled && !g_key.empty() && !g_model.empty();
    }

    void load() {
        g_learned.clear();

        auto json = file::readJson(cachePath());
        if (json.isErr()) {
            return;  // 아직 물어본 적이 없으면 파일도 없다
        }

        auto const root = json.unwrapOrDefault();
        if (!root.isObject()) {
            log::warn("the machine translation file is not an object");
            return;
        }

        for (auto const& [source, korean] : root) {
            if (korean.isString()) {
                g_learned.emplace(source, korean.asString().unwrapOrDefault());
            }
        }
        log::info("loaded {} machine translations", g_learned.size());
    }

    std::string const* find(std::string_view source) {
        auto const found = g_learned.find(source);
        return found == g_learned.end() ? nullptr : &found->second;
    }

    bool worthAsking(std::string_view text) {
        if (text.size() < 2 || text.size() > 120) {
            return false;
        }
        if (containsHangul(text)) {
            return false;
        }
        // "35/48" 이나 "100%" 처럼 글자가 없는 것은 물어볼 것이 없다.
        for (unsigned char byte : text) {
            if (std::isalpha(byte)) {
                return true;
            }
        }
        return false;
    }

    void request(std::string source, std::function<void(std::string const&)> onTranslated) {
        if (!enabled() || g_asked >= SESSION_LIMIT || g_inFlight >= MAX_IN_FLIGHT) {
            return;
        }
        if (g_learned.contains(source) || g_pending.contains(source) || g_refused.contains(source)) {
            return;
        }

        g_pending.insert(source);
        ++g_asked;
        ++g_inFlight;

        auto part = matjson::Value::object();
        part.set("text", matjson::Value(std::string(PROMPT) + source));
        auto parts = matjson::Value::array();
        parts.push(part);
        auto content = matjson::Value::object();
        content.set("parts", parts);
        auto contents = matjson::Value::array();
        contents.push(content);
        auto body = matjson::Value::object();
        body.set("contents", contents);

        // 열쇠가 주소에 들어가므로 이 주소는 어디에도 기록하지 않는다.
        auto url = fmt::format(
            "https://generativelanguage.googleapis.com/v1beta/models/{}:generateContent?key={}",
            g_model, g_key
        );

        async::spawn(
            [url = std::move(url), body = std::move(body)] {
                return web::WebRequest()
                    .bodyJSON(body)
                    .timeout(std::chrono::seconds(20))
                    .post(url);
            },
            [source = std::move(source), onTranslated = std::move(onTranslated)](
                web::WebResponse response
            ) {
                --g_inFlight;
                g_pending.erase(source);

                if (!response.ok()) {
                    g_refused.insert(source);
                    log::warn("Gemini refused a translation (HTTP {})", response.code());
                    return;
                }

                auto json = response.json();
                if (json.isErr()) {
                    g_refused.insert(source);
                    return;
                }

                // 없는 키를 짚으면 빈 값이 나오므로 중간 확인 없이 파고들어도 된다.
                auto const root = json.unwrapOrDefault();
                auto const korean = tidy(
                    root["candidates"][0]["content"]["parts"][0]["text"].asString().unwrapOrDefault()
                );

                // 답이 원문 그대로면 모델이 고유명사로 본 것이다. 그대로 둔다.
                if (korean.empty() || korean == source || !containsHangul(korean)) {
                    g_refused.insert(source);
                    return;
                }

                auto const [entry, added] = g_learned.emplace(source, korean);
                save();
                onTranslated(entry->second);
            }
        );
    }
}
