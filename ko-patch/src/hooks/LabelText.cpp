#include <Geode/Geode.hpp>
#include <Geode/modify/CCLabelBMFont.hpp>

#include "Translator.hpp"

using namespace geode::prelude;

namespace {
    // 비트맵 글꼴 경로는 한 번만 만들어 둔다.
    std::string const& ownFont() {
        static std::string const path = "neodgm.fnt"_spr;
        return path;
    }
}

// GD 가 화면에 글자를 올릴 때는 거의 전부 이 한 지점을 지난다. 수도관이 여러
// 갈래로 갈라지기 전의 본관을 잡는 셈이라, 여기 하나만 막으면 메뉴든 팝업이든
// 같은 방식으로 걸린다. 라벨을 만들 때 쓰는 initWithString 도 안에서 이 함수를
// 부르므로 따로 후킹하지 않아도 된다.
class $modify(KoreanLabel, CCLabelBMFont) {
    struct Fields {
        // setFntFile 이 내부에서 다시 setString 을 불러서 무한히 되도는 것을 막는다.
        bool m_swappingFont = false;
        bool m_usingOwnFont = false;
    };

    void setString(char const* text, bool needUpdateLabel) {
        auto const& translator = kopatch::Translator::get();

        if (!text || m_fields->m_swappingFont || !translator.enabled()) {
            CCLabelBMFont::setString(text, needUpdateLabel);
            return;
        }

        // 이미 한글인 글자는 다른 한국어 패치가 먼저 바꿔 놓은 것이다. 거기에
        // 또 손을 대면 두 패치가 같은 라벨을 두고 서로 밀어내게 된다.
        if (kopatch::containsHangul(text)) {
            CCLabelBMFont::setString(text, needUpdateLabel);
            return;
        }

        auto const* entry = translator.find(text);
        if (!entry) {
            CCLabelBMFont::setString(text, needUpdateLabel);
            return;
        }

        if (entry->korean && translator.ownFont() && !m_fields->m_usingOwnFont) {
            m_fields->m_swappingFont = true;
            this->setFntFile(ownFont().c_str());
            m_fields->m_swappingFont = false;
            m_fields->m_usingOwnFont = true;
        }

        CCLabelBMFont::setString(entry->text.c_str(), needUpdateLabel);
    }
};
