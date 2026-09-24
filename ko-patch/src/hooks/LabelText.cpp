#include <Geode/Geode.hpp>
#include <Geode/modify/CCLabelBMFont.hpp>

#include "Translator.hpp"

using namespace geode::prelude;

namespace {
    // 비트맵 글꼴 경로는 한 번만 만들어 둔다. 고른 글꼴과 금색 여부의
    // 네 갈래다.
    std::string const& ownFont(bool pixel, bool gold) {
        static std::string const juaPlain = "jua.fnt"_spr;
        static std::string const juaGold = "jua-gold.fnt"_spr;
        static std::string const pixelPlain = "neodgm.fnt"_spr;
        static std::string const pixelGold = "neodgm-gold.fnt"_spr;

        if (pixel) {
            return gold ? pixelGold : pixelPlain;
        }
        return gold ? juaGold : juaPlain;
    }

    // GD 는 제목과 강조에 금색 글꼴을 쓴다. 번역했다고 전부 흰 글꼴로 바꿔
    // 버리면 제목과 본문이 같아 보여서, 화면의 위아래가 구분되지 않는다.
    bool wantsGold(std::string_view fontFile) {
        return fontFile.find("gold") != std::string_view::npos;
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

        auto const entry = translator.translate(text);
        if (!entry) {
            CCLabelBMFont::setString(text, needUpdateLabel);
            return;
        }

        if (entry->korean && translator.ownFont() && !m_fields->m_usingOwnFont) {
            // 어느 글꼴을 쓰고 있었는지는 바꾸기 전에 봐야 한다. 바꾸고 나면
            // 우리 글꼴로 덮여서 원래 무엇이었는지 알 수 없다.
            std::string_view const current = m_sFntFile;

            m_fields->m_swappingFont = true;
            this->setFntFile(ownFont(translator.pixelFont(), wantsGold(current)).c_str());
            m_fields->m_swappingFont = false;
            m_fields->m_usingOwnFont = true;
        }

        CCLabelBMFont::setString(entry->text.c_str(), needUpdateLabel);
    }
};
