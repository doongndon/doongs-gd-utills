#include "Translator.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <string>
#include <vector>

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

    // 셈하는 말. 번역문에서 빈칸 뒤에 이런 말이 붙어 있으면 그 빈칸은 숫자
    // 자리다.
    constexpr std::string_view COUNTERS[] = {
        "개", "명", "곡", "번", "쪽", "점", "초", "분", "일", "해", "달",
        "시간", "층", "줄", "칸", "가지", "도", "위",
    };

    // 숫자, 또는 아직 숫자가 채워지지 않은 자리표(%i, %.2f 같은 것).
    bool isNumeric(std::string_view text) {
        if (text.size() >= 2 && text.front() == '%') {
            std::size_t i = 1;
            while (i < text.size()
                   && (std::isdigit(static_cast<unsigned char>(text[i])) || text[i] == '.'
                       || text[i] == '-' || text[i] == '+' || text[i] == ' ' || text[i] == '#')) {
                ++i;
            }
            return i + 1 == text.size() && std::isalpha(static_cast<unsigned char>(text[i]));
        }
        bool digit = false;
        for (unsigned char byte : text) {
            if (std::isdigit(byte)) digit = true;
            else if (byte != ',' && byte != '.' && byte != '-' && byte != '+' && byte != ' ') {
                return false;
            }
        }
        return digit;
    }

    // "{} Levels" 의 빈칸은 숫자 자리인데, 빈칸은 무엇이든 받아들인다. 그래서
    // "Increase Maximum Levels" 가 통째로 걸려 화면에 "레벨 Increase Maximum개"
    // 가 나왔다. 자물쇠가 헐거우면 열쇠가 아닌 것도 들어간다.
    //
    // 어느 빈칸이 숫자 자리인지는 번역문이 이미 말해 주고 있다. 뒤에 "개" 나
    // "명" 이 붙어 있으면 세는 자리다. 그 자리에는 숫자만 들인다.
    bool wantsNumber(std::string const& replacement, std::size_t after) {
        if (after < replacement.size() && replacement[after] == ' ') ++after;
        std::string_view const rest(replacement.data() + after, replacement.size() - after);
        for (auto const& counter : COUNTERS) {
            if (rest.starts_with(counter)) return true;
        }
        return false;
    }

    // 번역문의 빈칸 하나. 한국어는 영어와 말의 차례가 다르므로 {0} {1} 처럼
    // 번호를 붙여 자리를 바꿔 넣을 수 있어야 한다. "Collect 5 Fire Shards to
    // unlock this Cube!" 를 차례대로 채우면 "이 Collect 5 Fire Shards 을(를)
    // 열려면 Cube" 가 된다. 번호가 없으면 예전처럼 차례대로 채운다.
    struct Slot {
        std::size_t at;
        std::size_t length;
        std::size_t index;
    };

    std::vector<Slot> slotsOf(std::string const& replacement) {
        std::vector<Slot> slots;
        std::size_t next = 0;
        for (std::size_t i = 0; i + 1 < replacement.size(); ++i) {
            if (replacement[i] != '{') continue;
            std::size_t j = i + 1;
            while (j < replacement.size() && std::isdigit(static_cast<unsigned char>(replacement[j]))) {
                ++j;
            }
            if (j >= replacement.size() || replacement[j] != '}') continue;
            std::size_t index = next;
            if (j > i + 1) {
                index = static_cast<std::size_t>(std::stoul(replacement.substr(i + 1, j - i - 1)));
            }
            else {
                ++next;
            }
            slots.push_back(Slot{ .at = i, .length = j - i + 1, .index = index });
            i = j;
        }
        return slots;
    }

    // 틀 안에서 또 틀을 찾다가 제자리를 도는 일이 없게 한 겹만 허락한다.
    thread_local int g_depth = 0;

    // 한국어 조사는 앞말의 받침에 따라 갈린다. 빈칸에 무엇이 들어올지 모르니
    // 표에는 "을(를)" 처럼 둘 다 적어 두고, 채운 뒤에 고른다. "큐브 을(를)" 이
    // 아니라 "큐브를" 이 되도록.
    struct Particle {
        std::string_view both;     // 표에 적힌 꼴
        std::string_view withEnd;  // 받침이 있을 때
        std::string_view noEnd;    // 받침이 없을 때
    };

    constexpr Particle PARTICLES[] = {
        { "을(를)", "을", "를" }, { "를(을)", "을", "를" },
        { "이(가)", "이", "가" }, { "가(이)", "이", "가" },
        { "은(는)", "은", "는" }, { "는(은)", "은", "는" },
        { "와(과)", "과", "와" }, { "과(와)", "과", "와" },
        { "으로(로)", "으로", "로" }, { "로(으로)", "으로", "로" },
    };

    // 0 = 받침 없음, 1 = 받침 있음, 2 = 알 수 없음(그대로 두기)
    int endsWithConsonant(std::string const& text, std::size_t before) {
        // 조사 앞의 공백과 <...> 꼬리표를 건너뛴다. 색깔 표시가 사이에 끼어 있다.
        std::size_t at = before;
        for (;;) {
            while (at > 0 && text[at - 1] == ' ') --at;
            if (at > 0 && text[at - 1] == '>') {
                auto const open = text.rfind('<', at - 1);
                if (open == std::string::npos) break;
                at = open;
                continue;
            }
            break;
        }
        if (at == 0) return 2;

        // 마지막 글자 하나를 UTF-8 에서 거꾸로 떼어낸다.
        std::size_t start = at - 1;
        while (start > 0 && (static_cast<unsigned char>(text[start]) & 0xC0) == 0x80) --start;
        auto const length = at - start;
        unsigned int code = 0;
        unsigned char const first = static_cast<unsigned char>(text[start]);
        if (length == 1) code = first;
        else if (length == 2) code = first & 0x1F;
        else if (length == 3) code = first & 0x0F;
        else if (length == 4) code = first & 0x07;
        else return 2;
        for (std::size_t i = 1; i < length; ++i) {
            code = (code << 6) | (static_cast<unsigned char>(text[start + i]) & 0x3F);
        }

        if (code >= 0xAC00 && code <= 0xD7A3) {
            return (code - 0xAC00) % 28 == 0 ? 0 : 1;
        }
        if (code >= '0' && code <= '9') {
            // 영 일 이 삼 사 오 육 칠 팔 구
            constexpr int HAS_END[] = { 1, 1, 0, 1, 0, 0, 1, 1, 1, 0 };
            return HAS_END[code - '0'];
        }
        return 2;
    }

    // ㄹ 받침 뒤에서는 "으로" 가 아니라 "로" 를 쓴다.
    bool endsWithRieul(std::string const& text, std::size_t before) {
        std::size_t at = before;
        while (at > 0 && text[at - 1] == ' ') --at;
        if (at < 3) return false;
        std::size_t start = at - 1;
        while (start > 0 && (static_cast<unsigned char>(text[start]) & 0xC0) == 0x80) --start;
        if (at - start != 3) return false;
        unsigned int code = static_cast<unsigned char>(text[start]) & 0x0F;
        code = (code << 6) | (static_cast<unsigned char>(text[start + 1]) & 0x3F);
        code = (code << 6) | (static_cast<unsigned char>(text[start + 2]) & 0x3F);
        if (code < 0xAC00 || code > 0xD7A3) return false;
        return (code - 0xAC00) % 28 == 8;  // ㄹ
    }

    void chooseParticles(std::string& text) {
        for (auto const& particle : PARTICLES) {
            for (auto at = text.find(particle.both); at != std::string::npos;
                 at = text.find(particle.both, at)) {
                int const kind = endsWithConsonant(text, at);
                if (kind == 2) {
                    at += particle.both.size();
                    continue;
                }
                std::string_view pick = kind == 1 ? particle.withEnd : particle.noEnd;
                if (particle.withEnd == "으로" && kind == 1 && endsWithRieul(text, at)) {
                    pick = particle.noEnd;
                }
                // 조사 앞의 공백도 함께 거둔다. "큐브 를" 이 아니라 "큐브를".
                std::size_t begin = at;
                while (begin > 0 && text[begin - 1] == ' ') --begin;
                text.replace(begin, at + particle.both.size() - begin, pick);
                at = begin + pick.size();
            }
        }
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
            chooseParticles(text);
            // 줄바꿈이 공백으로 바뀌어 들어오는 자리가 있어 그 꼴도 함께 담는다.
            if (english.find('\n') != std::string::npos) {
                std::string flat(english);
                std::ranges::replace(flat, '\n', ' ');
                m_table.emplace(flat, Entry{ .text = text, .korean = isKorean });
            }
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
            if (match.find('\n') != std::string::npos) {
                std::string flat(match);
                std::ranges::replace(flat, '\n', ' ');
                m_patterns.push_back(Pattern{
                    .segments = split(flat),
                    .replacement = to,
                    .korean = isKorean,
                });
            }
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
        if (g_depth > 1) {
            return std::nullopt;
        }
        ++g_depth;
        struct Unwind { ~Unwind() { --g_depth; } } const unwind;

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
            auto const slots = slotsOf(pattern.replacement);

            // 세는 자리에 문장이 밀려들었는지 번역문을 보고 가린다.
            bool wrongKind = false;
            for (auto const& slot : slots) {
                if (slot.index < captures.size()
                    && wantsNumber(pattern.replacement, slot.at + slot.length)
                    && !isNumeric(captures[slot.index])) {
                    wrongKind = true;
                    break;
                }
            }
            if (wrongKind) {
                continue;
            }

            std::string result;
            result.reserve(pattern.replacement.size() + text.size());
            std::size_t start = 0;
            for (auto const& slot : slots) {
                result.append(pattern.replacement, start, slot.at - start);
                if (slot.index < captures.size()) {
                    // 떼어낸 조각도 번역표를 거친다. "Fire Gauntlet" 이 "Fire
                    // 건틀릿" 처럼 반만 한국어로 남는 것을 막아 주고, "Collect 5
                    // Fire Shards" 처럼 조각 자체가 틀에 걸리는 문장도 한국어가
                    // 된다.
                    auto const piece = this->lookup(captures[slot.index]);
                    result.append(piece ? std::string_view(piece->text) : captures[slot.index]);
                }
                start = slot.at + slot.length;
            }
            result.append(pattern.replacement, start, std::string::npos);
            chooseParticles(result);

            return Entry{ .text = std::move(result), .korean = pattern.korean };
        }

        return std::nullopt;
    }
}
