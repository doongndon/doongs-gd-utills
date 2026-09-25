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
        std::vector<bool> numeric;          // {#} 로 적은 자리는 숫자만 받는다
        std::string replacement;            // {} 가 그대로 남아 있는 번역문
        bool korean = false;
    };

    class Translator {
    public:
        static Translator& get();

        void load();

        // 설치된 모드의 이름과 만든 이를 표에서 제외한다. 남이 지은 이름이라
        // 번역할 것이 아니고, "Save Buttons" 같은 이름은 우리 틀에 걸려
        // "Buttons 저장" 이 되어 버린다.
        void protectModNames();

        // 남이 지은 이름인가. 미번역 목록에도 섞이지 않게 한다.
        bool isProtected(std::string_view text) const { return m_protected.contains(text); }

        void setEnabled(bool enabled) { m_enabled = enabled; }
        bool enabled() const { return m_enabled && !m_table.empty(); }

        // 끄면 라벨의 글꼴을 건드리지 않는다. 한국어 텍스처팩처럼 이미 한글이
        // 나오는 글꼴을 쓰고 있다면, 우리 글꼴로 바꿔 끼우는 순간 한 화면에
        // 두 글꼴이 섞여 보인다.
        void setOwnFont(bool own) { m_ownFont = own; }
        bool ownFont() const { return m_ownFont; }

        // 설정에서 고른 글꼴. 둥근모꼴은 픽셀 텍스처팩과, 주아는 대부분의
        // 한국어 팩과 어울린다.
        void setPixelFont(bool pixel) { m_pixelFont = pixel; }
        bool pixelFont() const { return m_pixelFont; }

        std::optional<Entry> translate(std::string_view text) const;

    private:
        void loadExact(matjson::Value const& section);
        void loadPatterns(matjson::Value const& section);
        void loadNames(matjson::Value const& section);
        std::optional<Entry> lookup(std::string_view text) const;
        std::optional<Entry> applyPatterns(std::string_view text) const;

        geode::utils::StringMap<Entry> m_table;
        geode::utils::StringSet m_protected;
        // 표에 적어 둔, 번역하지 않을 이름들
        geode::utils::StringSet m_keepEnglish;
        std::vector<Pattern> m_patterns;
        bool m_enabled = true;
        bool m_ownFont = true;
        bool m_pixelFont = false;
    };
}
