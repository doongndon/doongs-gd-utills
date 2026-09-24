#include "Translator.hpp"

#include <algorithm>

using namespace geode::prelude;

namespace {
    constexpr std::string_view PLACEHOLDER = "{}";

    // 틀 문장에서 {} 로 잘라낸 조각들. 앞뒤가 비어 있을 수도 있다.
    std::vector<std::string> split(std::string const& pattern) {
        std::vector<std::string> segments;
        std::size_t start = 0;
        for (auto at = pattern.find(PLACEHOLDER); at != std::string::npos;
             at = pattern.find(PLACEHOLDER, start)) {
            segments.push_back(pattern.substr(start, at - start));
            start = at + PLACEHOLDER.size();
        }
        segments.push_back(pattern.substr(start));
        return segments;
    }

    // {} 자리에 들어간 원문은 우리가 만든 글자가 아니다. 아틀라스에 없는 글자가
    // 섞여 있으면 화면에서 그 부분이 빈칸이 되므로, 확실히 그릴 수 있는 범위일
    // 때만 바꾼다.
    bool isPlainAscii(std::string_view text) {
        for (unsigned char byte : text) {
            if (byte < 0x20 || byte > 0x7E) {
                return false;
            }
        }
        return true;
    }
}

namespace kopatch {
    // 한글 음절 U+AC00..U+D7A3 은 UTF-8 에서 선두 바이트가 0xEA..0xED 이다.
    bool containsHangul(std::string_view text) {
        for (unsigned char byte : text) {
            if (byte >= 0xEA && byte <= 0xED) {
                return true;
            }
        }
        return false;
    }

    Translator& Translator::get() {
        static Translator instance;
        return instance;
    }

    void Translator::load() {
        m_table.clear();
        m_patterns.clear();

        auto json = file::readJson(Mod::get()->getResourcesDir() / "ko.json");
        if (json.isErr()) {
            log::error("could not read the Korean translation table");
            return;
        }

        auto const root = json.unwrapOrDefault();
        if (!root.isObject()) {
            log::error("the Korean translation table is not an object");
            return;
        }

        auto const exact = root["exact"];
        if (exact.isObject()) {
            for (auto const& [english, korean] : exact) {
                if (!korean.isString()) {
                    continue;
                }
                auto text = korean.asString().unwrapOrDefault();
                if (english.empty() || text.empty()) {
                    continue;
                }
                bool const isKorean = containsHangul(text);
                m_table.emplace(english, Entry{ .text = std::move(text), .korean = isKorean });
            }
        }

        auto const patterns = root["patterns"];
        if (patterns.isArray()) {
            for (auto const& rule : patterns.asArray().unwrapOrDefault()) {
                auto const match = rule["match"].asString().unwrapOrDefault();
                auto const to = rule["to"].asString().unwrapOrDefault();
                if (match.empty() || to.empty()) {
                    continue;
                }
                auto segments = split(match);
                if (segments.size() < 2) {
                    continue;  // {} 가 없으면 틀이 아니라 그냥 문장이다
                }
                m_patterns.push_back(Pattern{
                    .segments = std::move(segments),
                    .replacement = to,
                    .korean = containsHangul(to),
                });
            }
        }

        log::info("loaded {} translations and {} patterns", m_table.size(), m_patterns.size());
    }

    std::optional<Entry> Translator::translate(std::string_view text) const {
        auto const found = m_table.find(text);
        if (found != m_table.end()) {
            return found->second;
        }
        return this->applyPatterns(text);
    }

    std::optional<Entry> Translator::applyPatterns(std::string_view text) const {
        for (auto const& pattern : m_patterns) {
            auto const& segments = pattern.segments;

            if (!text.starts_with(segments.front()) || !text.ends_with(segments.back())) {
                continue;
            }

            // 조각들을 순서대로 찾아가며 그 사이에 낀 원문을 거둬들인다.
            std::vector<std::string_view> captures;
            std::size_t cursor = segments.front().size();
            bool matched = true;

            for (std::size_t i = 1; i < segments.size(); ++i) {
                auto const& segment = segments[i];
                std::size_t at;
                if (i + 1 == segments.size()) {
                    // 마지막 조각은 문자열 끝에 붙어 있어야 한다.
                    at = text.size() - segment.size();
                    if (at < cursor) {
                        matched = false;
                        break;
                    }
                }
                else {
                    at = text.find(segment, cursor);
                    if (at == std::string_view::npos || segment.empty()) {
                        matched = false;
                        break;
                    }
                }
                captures.push_back(text.substr(cursor, at - cursor));
                cursor = at + segment.size();
            }

            if (!matched || captures.empty()) {
                continue;
            }
            if (!std::ranges::all_of(captures, isPlainAscii)) {
                continue;
            }

            std::string result;
            result.reserve(pattern.replacement.size() + text.size());
            std::size_t start = 0;
            std::size_t index = 0;
            for (auto at = pattern.replacement.find(PLACEHOLDER); at != std::string::npos;
                 at = pattern.replacement.find(PLACEHOLDER, start)) {
                result.append(pattern.replacement, start, at - start);
                if (index < captures.size()) {
                    // 떼어낸 조각도 번역표를 거친다. "Fire Gauntlet" 이 "Fire
                    // 건틀릿" 처럼 반만 한국어로 남는 것을 막아 준다.
                    auto const known = m_table.find(captures[index]);
                    result.append(
                        known != m_table.end() ? std::string_view(known->second.text)
                                               : captures[index]
                    );
                }
                ++index;
                start = at + PLACEHOLDER.size();
            }
            result.append(pattern.replacement, start, std::string::npos);

            return Entry{ .text = std::move(result), .korean = pattern.korean };
        }

        return std::nullopt;
    }
}
