#include "Translator.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace {
    constexpr std::string_view PLACEHOLDER = "{}";

    // 틀 문장을 빈칸으로 자른 조각들. 앞뒤가 비어 있을 수도 있다. 빈칸은 {} 가
    // 보통이고, {#} 은 "여기에는 숫자만" 이라는 뜻이다. "{} to {} of {}" 는
    // "1 to 10 of 50" 을 위한 틀인데 빈칸이 아무것이나 받는 바람에 "Move this
    // level to the top of the levels list?" 까지 삼켰다. 숫자 자리라고 적어 두면
    // 그런 일이 없다.
    struct Split {
        std::vector<std::string> segments;
        std::vector<bool> numeric;
    };

    Split split(std::string const& pattern) {
        Split out;
        std::size_t start = 0;
        for (std::size_t i = 0; i + 1 < pattern.size(); ++i) {
            if (pattern[i] != '{') continue;
            bool digits = false;
            std::size_t end = 0;
            if (pattern[i + 1] == '}') end = i + 2;
            else if (i + 2 < pattern.size() && pattern[i + 1] == '#' && pattern[i + 2] == '}') {
                digits = true;
                end = i + 3;
            }
            else continue;
            out.segments.push_back(pattern.substr(start, i - start));
            out.numeric.push_back(digits);
            start = end;
            i = end - 1;
        }
        out.segments.push_back(pattern.substr(start));
        return out;
    }

    // {} 자리에 들어간 원문은 우리가 만든 글자가 아니다. 아틀라스에 없는 글자가
    // 섞여 있으면 화면에서 그 부분이 빈칸이 되므로, 확실히 그릴 수 있는 범위일
    // 때만 바꾼다.
    // 조각 하나가 담아도 되는 색표는 한 벌뿐이다. 여는 표와 닫는 표의
    // 수가 다르거나 닫는 표가 둘 이상이면 문장을 삼킨 것이다.
    bool tooManyTags(std::string_view capture) {
        std::size_t opens = 0;
        std::size_t closes = 0;
        for (std::size_t i = capture.find('<'); i != std::string_view::npos;
             i = capture.find('<', i + 1)) {
            auto const end = capture.find('>', i);
            if (end == std::string_view::npos) break;
            auto const tag = capture.substr(i + 1, end - i - 1);
            if (tag == "/c") ++closes;
            else if (tag.size() >= 2 && tag[0] == 'c'
                     && (tag.size() == 2 || tag[1] == '-')) ++opens;
        }
        return closes > 1 || opens != closes;
    }

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

    // 이름은 번역하지 않는다. 레벨과 노래와 사람의 이름은 뜻을 옮길 것이 아니라
    // 부르는 말이고, 그것들은 다른 글자와 똑같은 라벨을 지나가므로 표에 우연히
    // 같은 낱말이 있으면 엉뚱하게 바뀐다. "Silence" 라는 레벨이 "무음" 이 되는 식이다.
    void Translator::loadNames(matjson::Value const& section) {
        for (auto const& entry : section) {
            if (!entry.isString()) continue;
            auto name = entry.asString().unwrapOrDefault();
            if (!name.empty()) m_keepEnglish.emplace(std::move(name));
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
            auto cut = split(match);
            if (cut.segments.size() < 2) {
                continue;  // 빈칸이 없으면 틀이 아니라 그냥 문장이다
            }
            bool const isKorean = containsHangul(to);
            if (match.find('\n') != std::string::npos) {
                std::string flat(match);
                std::ranges::replace(flat, '\n', ' ');
                auto flatCut = split(flat);
                m_patterns.push_back(Pattern{
                    .segments = std::move(flatCut.segments),
                    .numeric = std::move(flatCut.numeric),
                    .replacement = to,
                    .korean = isKorean,
                });
            }
            m_patterns.push_back(Pattern{
                .segments = std::move(cut.segments),
                .numeric = std::move(cut.numeric),
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
        m_keepEnglish.clear();

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
            if (name == "exact" && section.isObject()) {
                this->loadExact(section);
            }
            else if (name == "patterns" && section.isObject()) {
                this->loadPatterns(section);
            }
            else if (name == "names" && section.isArray()) {
                this->loadNames(section);
            }
        }

        log::info("loaded {} translations, {} patterns, {} names left alone",
                  m_table.size(), m_patterns.size(), m_keepEnglish.size());
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
        if (m_keepEnglish.contains(text) || m_protected.contains(text)) {
            return std::nullopt;
        }

        if (auto entry = this->lookup(text)) {
            return entry;
        }

        // 단추에 글자가 길면 GD 가 줄을 바꿔 넣는다. 그 줄바꿈은 글자의 일부라
        // "Disable Trigger\nOrb Scale" 은 표에 적힌 "Disable Trigger Orb Scale"
        // 과 다른 문자열이 된다. 같은 말인데 표가 못 알아보는 셈이라, 줄바꿈을
        // 띄어쓰기로 펴서 한 번 더 찾아본다.
        // 줄바꿈이 딱 하나일 때만 편다. 그 하나는 단추에 글자가 길어 GD 가
        // 끼워 넣은 줄바꿈이고, 편 문장은 여전히 한 문장이다.
        //
        // 여러 줄짜리 글을 펴면 안 된다. 펴는 순간 모든 줄이 한 줄이 되고,
        // 끝이 {} 로 열린 틀 하나가 그 전부를 삼켜 버린다. BetterInfo 의
        // 기록 창이 그렇게 "시도 (일반): 14 Attempts (practice): 197 ..." 하고
        // 한 줄로 뭉개졌다. 첫 줄만 한국어가 되고 나머지는 그 안에 갇힌다.
        auto const firstBreak = text.find('\n');
        if (firstBreak == std::string_view::npos
            || text.find('\n', firstBreak + 1) != std::string_view::npos) {
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
            // 빈칸에 담기는 것은 값이다. 값 하나가 제 색을 입고 오는 것은
            // 흔한 일이라 (상점의 "<cl>마나 오브</c>" 가 그렇다) 막으면 안 된다.
            // 다만 한 조각 안에 색표가 여러 벌 들어 있다면 그것은 값이 아니라
            // 여러 문장이고, 그 틀은 제 자리보다 멀리까지 삼킨 것이다.
            // (BetterInfo 의 기록 창이 그렇게 한 줄로 뭉개졌다.)
            if (std::ranges::any_of(captures, tooManyTags)) {
                continue;
            }
            auto const slots = slotsOf(pattern.replacement);

            // 세는 자리에 문장이 밀려들었는지 가린다. 틀에 {#} 이라 적혀 있거나,
            // 번역문에서 빈칸 뒤에 세는 말이 붙어 있으면 숫자 자리다.
            bool wrongKind = false;
            for (std::size_t i = 0; i < captures.size() && i < pattern.numeric.size(); ++i) {
                if (pattern.numeric[i] && !isNumeric(captures[i])) {
                    wrongKind = true;
                    break;
                }
            }
            for (auto const& slot : slots) {
                if (wrongKind) break;
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
                    // translate 를 거쳐야 한다. lookup 은 영어로 두기로 한
                    // 이름 목록을 보지 않으므로, 여기서 lookup 을 부르면
                    // "'Can't Let Go' 일반 모드로 완료함" 이 "'캔트 렛 고'" 가
                    // 되어 버린다. 레벨 이름은 문장 안에서도 이름이다.
                    auto const piece = this->translate(captures[slot.index]);
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
