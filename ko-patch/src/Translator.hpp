#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/StringMap.hpp>

#include <optional>
#include <string>
#include <vector>

namespace kopatch {
    // UTF-8 문자열에 한글 음절이 섞여 있는지 본다.
    bool containsHangul(std::string_view text);

    struct Entry {
        std::string text;
        // 한글이 섞여 있으면 라벨의 글꼴도 바꿔 끼워야 한다.
        bool korean = false;
    };

    // 레벨 이름만 바뀌고 틀은 같은 문장을 위한 규칙.
    // "Complete '{}' in Normal mode" 처럼 {} 자리에 무엇이 와도 걸리게 한다.
    struct Pattern {
        std::vector<std::string> segments;  // 원문을 {} 로 자른 조각들
        std::string replacement;            // {} 가 그대로 남아 있는 번역문
        bool korean = false;
    };

    class Translator {
    public:
        static Translator& get();

        void load();

        void setEnabled(bool enabled) { m_enabled = enabled; }
        bool enabled() const { return m_enabled && !m_table.empty(); }

        // 끄면 라벨의 글꼴을 건드리지 않는다. 한국어 텍스처팩처럼 이미 한글이
        // 나오는 글꼴을 쓰고 있다면, 우리 글꼴로 바꿔 끼우는 순간 한 화면에
        // 두 글꼴이 섞여 보인다.
        void setOwnFont(bool own) { m_ownFont = own; }
        bool ownFont() const { return m_ownFont; }

        std::optional<Entry> translate(std::string_view text) const;

    private:
        std::optional<Entry> applyPatterns(std::string_view text) const;

        geode::utils::StringMap<Entry> m_table;
        std::vector<Pattern> m_patterns;
        bool m_enabled = true;
        bool m_ownFont = true;
    };
}
