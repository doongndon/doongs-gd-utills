#include "Translator.hpp"

#include <algorithm>
#include <cctype>
#include <functional>

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

    // "{} Levels" 같은 짧은 틀은 빈칸이 숫자 자리다. 그런데 빈칸은 무엇이든
    // 받아들이므로 "Increase Maximum Levels" 가 통째로 걸려 화면에
    // "레벨 Increase Maximum개" 가 나왔다. 자물쇠가 헐거우면 열쇠가 아닌 것도
    // 들어간다. 틀에 남은 글자가 몇 자 안 되는데 빈칸에 영어 낱말이 둘 이상
    // 들어앉았다면, 숫자가 올 자리에 문장이 밀려든 것이다.
    bool isEnglishPhrase(std::string_view text) {
        bool letter = false;
        bool space = false;
        for (unsigned char byte : text) {
            if (byte == ' ') space = true;
            else if (std::isalpha(byte)) letter = true;
        }
        return letter && space;
    }

    std::size_t literalLength(std::vector<std::string> const& segments) {
        std::size_t total = 0;
        for (auto const& segment : segments) total += segment.size();
        return total;
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

    void Translator::loadExact(matjson::Value const& section) {
        for (auto const& [english, korean] : section) {
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

    void Translator::loadPatterns(matjson::Value const& section) {
        for (auto const& [match, korean] : section) {
            if (!korean.isString()) {
                continue;
            }
            auto to = korean.asString().unwrapOrDefault();
            if (match.empty() || to.empty()) {
                continue;
            }
            auto segments = split(match);
            if (segments.size() < 2) {
                continue;  // {} 가 없으면 틀이 아니라 그냥 문장이다
            }
            bool const isKorean = containsHangul(to);
            m_patterns.push_back(Pattern{
                .segments = std::move(segments),
                .replacement = std::move(to),
                .korean = isKorean,
            });
        }

        // 글자가 고정된 부분이 긴 틀부터 본다. 두 틀이 같은 문장에 걸릴 때 더
        // 깐깐한 쪽이 이기고, JSON 에 적힌 순서와 무관하게 늘 같은 결과가 나온다.
        std::ranges::sort(m_patterns, std::ranges::greater{}, [](Pattern const& pattern) {
            std::size_t length = 0;
            for (auto const& segment : pattern.segments) {
                length += segment.size();
            }
            return length;
        });
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

        for (auto const& [name, section] : root) {
            if (!section.isObject()) {
                continue;
            }
            if (name == "exact") {
                this->loadExact(section);
            }
            else if (name == "patterns") {
                this->loadPatterns(section);
            }
        }

        log::info("loaded {} translations and {} patterns", m_table.size(), m_patterns.size());
    }

    void Translator::protectModNames() {
        m_protected.clear();
        for (auto* mod : Loader::get()->getAllMods()) {
            if (!mod) {
                continue;
            }
            m_protected.emplace(std::string(mod->getName().view()));
            for (auto const& developer : mod->getDevelopers()) {
                m_protected.emplace(developer);
            }
        }
        log::info("leaving {} mod names and developers alone", m_protected.size());
    }

    std::optional<Entry> Translator::lookup(std::string_view text) const {
        auto const found = m_table.find(text);
        if (found != m_table.end()) {
            return found->second;
        }
        return this->applyPatterns(text);
    }

    std::optional<Entry> Translator::translate(std::string_view text) const {
        // 남이 만든 모드의 이름과 제작자 이름은 건드리지 않는다.
        if (m_protected.contains(text)) {
            return std::nullopt;
        }

        if (auto entry = this->lookup(text)) {
            return entry;
        }

        // 단추에 글자가 길면 GD 가 줄을 바꿔 넣는다. 그 줄바꿈은 글자의 일부라
        // "Disable Trigger\nOrb Scale" 은 표에 적힌 "Disable Trigger Orb Scale"
        // 과 다른 문자열이 된다. 같은 말인데 표가 못 알아보는 셈이라, 줄바꿈을
        // 띄어쓰기로 펴서 한 번 더 찾아본다.
        if (text.find('\n') == std::string_view::npos) {
            return std::nullopt;
        }

        std::string flattened(text);
        std::ranges::replace(flattened, '\n', ' ');
        return this->lookup(flattened);
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
            if (literalLength(segments) < 20
                && std::ranges::any_of(captures, isEnglishPhrase)) {
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
