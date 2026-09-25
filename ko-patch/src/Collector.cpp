#include "Collector.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/AchievementManager.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/general.hpp>

#include <algorithm>
#include <cctype>
#include <memory>
#include <set>
#include <vector>

#include <fmt/format.h>

#include "Translator.hpp"

using namespace geode::prelude;

namespace {
    constexpr std::size_t LIMIT = 4000;      // 한 번에 담아 둘 줄 수
    constexpr std::size_t FLUSH_EVERY = 20;  // 이만큼 새로 들어오면 파일에 쓴다
    constexpr std::size_t CLIPBOARD_MAX = 15000;

    bool g_enabled = false;
    std::size_t g_sinceWrite = 0;

    // 차례대로 담아 둔다. 어떤 글의 앞토막은 그 글 바로 앞자리에 오므로,
    // 이웃만 보면 앞토막인지 알 수 있다.
    std::set<std::string, std::less<>>& seen() {
        static std::set<std::string, std::less<>> set;
        return set;
    }

    std::filesystem::path path() {
        return Mod::get()->getConfigDir() / "missing.txt";
    }

    // 띄어쓰기 없이 길고 글자와 숫자가 섞인 것은 사람이 읽는 말이 아니라
    // 열쇠나 해시다. 설정 칸에 적어 둔 API 키도 라벨을 지나가므로 여기서 막는다.
    // 한 번 새어 나간 열쇠는 주워 담을 수 없다.
    bool looksLikeSecret(std::string_view text) {
        if (text.size() < 20 || text.find(' ') != std::string_view::npos) return false;
        bool digit = false;
        bool letter = false;
        for (unsigned char byte : text) {
            if (std::isdigit(byte)) digit = true;
            else if (std::isalpha(byte)) letter = true;
        }
        return digit && letter;
    }

    // 화면에 뜨는 글만 남긴다. 저장 열쇠나 그림 이름, 주소 조각은 번역할 것이
    // 아니라서, 목록에 섞이면 정작 볼 것이 묻힌다.
    bool worthKeeping(std::string_view text) {
        if (text.size() < 2 || text.size() > 200) return false;
        if (kopatch::containsHangul(text)) return false;
        if (text.starts_with("http") || text.starts_with("&") || text.starts_with("--")) {
            return false;
        }

        bool letter = false;
        bool space = false;
        for (unsigned char byte : text) {
            if (byte < 0x20 || byte > 0x7E) return false;  // 영어 아닌 것은 우리 몫이 아니다
            if (std::isalpha(byte)) letter = true;
            if (byte == ' ') space = true;
        }
        if (!letter) return false;

        // 띄어쓰기가 없는데 / 나 _ 가 들어 있으면 사람이 읽을 글이 아니다.
        if (!space && (text.find('/') != std::string_view::npos
                       || text.find('_') != std::string_view::npos)) {
            return false;
        }
        return !looksLikeSecret(text);
    }
}

namespace kopatch::collector {
    void setEnabled(bool on) { g_enabled = on; }
    bool enabled() { return g_enabled; }

    std::size_t count() { return seen().size(); }

    void note(std::string_view text) {
        if (!g_enabled || seen().size() >= LIMIT) return;
        if (!worthKeeping(text)) return;
        if (Translator::get().isProtected(text)) return;  // 남의 모드 이름

        // Geode 는 한 줄을 그릴 때 낱말을 하나씩 덧붙이며 폭을 잰다. 그래서
        // " in the", " in the settings", " in the settings picks" 가 차례로
        // 여기를 지나간다. 전부 적어 두면 문장 하나가 목록 스무 줄이 된다.
        // 자라는 중인 글은 마지막 것만 남긴다.
        auto& set = seen();
        std::string incoming(text);

        auto after = set.lower_bound(incoming);
        if (after != set.end() && after->starts_with(incoming)) {
            return;  // 이미 더 자란 것이 있다
        }
        while (after != set.begin()) {
            auto before = std::prev(after);
            if (!incoming.starts_with(*before)) break;
            after = set.erase(before);
        }

        if (!set.insert(std::move(incoming)).second) return;

        if (++g_sinceWrite >= FLUSH_EVERY) {
            flush();
        }
    }

    void sweepAchievements() {
        auto* manager = AchievementManager::sharedState();
        if (!manager) return;
        auto* all = manager->getAllAchievements();
        if (!all) return;

        std::size_t added = 0;
        for (auto* object : CCArrayExt<CCObject*>(all)) {
            auto* entry = typeinfo_cast<CCDictionary*>(object);
            if (!entry) continue;

            for (auto const& [key, field] : CCDictionaryExt<std::string, CCObject*>(entry)) {
                std::string_view const name(key);
                // 이름과 설명만. 나머지는 그림 파일 이름이나 식별자다.
                if (name.find("itle") == std::string_view::npos
                    && name.find("escription") == std::string_view::npos) {
                    continue;
                }
                auto* text = typeinfo_cast<CCString*>(field);
                if (!text) continue;

                std::string value(text->getCString());
                if (value.empty() || kopatch::containsHangul(value)) continue;
                if (Translator::get().translate(value)) continue;  // 이미 표에 있다

                // 모으기를 켜지 않았어도 이 훑기는 담는다. 일부러 누른 것이므로.
                if (seen().insert(std::move(value)).second) ++added;
            }
        }
        log::info("swept {} untranslated achievement strings", added);
    }

    // 설치된 모드마다 자기 설명과 설정 이름을 들고 있다. 화면에 뜨기를
    // 기다릴 것 없이 한 번에 읽는다. 소스가 없는 모드도 이렇게는 읽힌다.
    void sweepMods() {
        std::size_t added = 0;
        auto take = [&added](std::string value) {
            if (value.empty() || kopatch::containsHangul(value)) return;
            if (looksLikeSecret(value)) return;
            if (Translator::get().translate(value)) return;
            if (seen().insert(std::move(value)).second) ++added;
        };

        for (auto* mod : Loader::get()->getAllMods()) {
            if (!mod) continue;
            if (auto const description = mod->getDescription()) take(*description);
            for (auto const& key : mod->getSettingKeys()) {
                auto const setting = mod->getSetting(key);
                if (!setting) continue;
                take(setting->getDisplayName());
                if (auto const description = setting->getDescription()) take(*description);

                // 고르는 칸의 보기와 버튼의 글자도 화면에 뜬다.
                if (auto const options = std::dynamic_pointer_cast<StringSettingV3>(setting)) {
                    if (auto const list = options->getEnumOptions()) {
                        for (auto const& option : *list) take(option);
                    }
                }
                if (auto const button = std::dynamic_pointer_cast<ButtonSettingV3>(setting)) {
                    for (auto const& [name, label] : button->getButtons()) take(label);
                }
            }
        }
        log::info("swept {} untranslated mod strings", added);
    }

    // 지금 화면에 떠 있는 것을 통째로 읽는다. 모으기를 켜기 전에 이미
    // 그려진 글은 훅을 지나간 적이 없으므로, 이 훑기만이 그것을 잡는다.
    void sweepScene() {
        std::size_t added = 0;
        auto take = [&added](std::string value) {
            if (value.empty() || kopatch::containsHangul(value)) return;
            if (looksLikeSecret(value)) return;
            if (Translator::get().translate(value)) return;
            if (seen().insert(std::move(value)).second) ++added;
        };

        auto walk = [&take](auto&& self, CCNode* node) -> void {
            if (!node) return;
            if (auto* label = typeinfo_cast<CCLabelBMFont*>(node)) {
                if (char const* text = label->getString()) take(text);
            }
            else if (auto* ttf = typeinfo_cast<CCLabelTTF*>(node)) {
                if (char const* text = ttf->getString()) take(text);
            }
            auto* children = node->getChildren();
            if (!children) return;
            for (auto* child : CCArrayExt<CCNode*>(children)) {
                self(self, child);
            }
        };

        walk(walk, CCDirector::sharedDirector()->getRunningScene());
        log::info("swept {} untranslated strings off the screen", added);
    }

    std::string flush() {
        g_sinceWrite = 0;

        std::vector<std::string_view> lines;
        lines.reserve(seen().size());
        for (auto const& line : seen()) {
            lines.push_back(line);
        }
        std::ranges::sort(lines);

        std::string out;
        for (auto const& line : lines) {
            out += line;
            out += '\n';
        }

        if (auto const written = file::writeStringSafe(path(), out); written.isErr()) {
            log::warn("could not write {}: {}", path(), written.unwrapErr());
        }
        return out;
    }

    void listenForButton() {
        ButtonSettingPressedEventV3(Mod::get(), "collect-copy")
            .listen([](std::string_view) {
                sweepAchievements();
                sweepMods();
                sweepScene();
                auto const text = flush();
                if (text.empty()) {
                    Notification::create(
                        "Nothing collected yet", NotificationIcon::Info, 4.f)->show();
                    return;
                }

                // 클립보드에 다 담기지 않을 만큼 길 수도 있다. 파일에는 전부 있다.
                std::string_view clipped(text);
                if (clipped.size() > CLIPBOARD_MAX) {
                    auto const cut = clipped.rfind('\n', CLIPBOARD_MAX);
                    clipped = clipped.substr(0, cut == std::string_view::npos ? 0 : cut + 1);
                }
                geode::utils::clipboard::write(std::string(clipped));

                auto const copied = std::ranges::count(clipped, '\n');
                Notification::create(
                    fmt::format("Copied {} of {} lines - all of them are in config/missing.txt",
                                copied, count()),
                    NotificationIcon::Success, 5.f)->show();
            })
            .leak();
    }
}
