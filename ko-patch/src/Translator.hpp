#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/StringMap.hpp>

#include <string>

namespace kopatch {
    // UTF-8 문자열에 한글 음절이 섞여 있는지 본다.
    bool containsHangul(std::string_view text);

    struct Entry {
        std::string text;
        // 한글이 섞여 있으면 라벨의 글꼴도 바꿔 끼워야 한다. 번역문은 우리가
        // 만드는 것이므로 불러올 때 한 번만 확인해 두고, 화면을 그릴 때마다
        // 글자를 훑지 않는다.
        bool korean = false;
    };

    class Translator {
    public:
        static Translator& get();

        // 모드 자원에 담긴 ko.json 을 읽는다.
        void load();

        void setEnabled(bool enabled) { m_enabled = enabled; }
        bool enabled() const { return m_enabled && !m_table.empty(); }

        // 끄면 라벨의 글꼴을 건드리지 않는다. 한국어 텍스처팩처럼 이미 한글이
        // 나오는 글꼴을 쓰고 있다면, 우리 글꼴로 바꿔 끼우는 순간 한 화면에
        // 두 글꼴이 섞여 보인다.
        void setOwnFont(bool own) { m_ownFont = own; }
        bool ownFont() const { return m_ownFont; }

        // 번역이 있으면 그 항목을, 없으면 nullptr 을 준다.
        Entry const* find(std::string_view text) const;

    private:
        geode::utils::StringMap<Entry> m_table;
        bool m_enabled = true;
        bool m_ownFont = true;
    };
}
