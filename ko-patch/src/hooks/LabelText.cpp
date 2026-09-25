#include <Geode/Geode.hpp>
#include <Geode/modify/CCLabelBMFont.hpp>

#include "Collector.hpp"
#include "Gemini.hpp"
#include "KoreanFont.hpp"
#include "Translator.hpp"

using namespace geode::prelude;

namespace {
    bool hasAsciiLetter(std::string_view text) {
        for (unsigned char byte : text) {
            if ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z')) {
                return true;
            }
        }
        return false;
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
        // 조각을 이어 붙여 문장을 만드는 중인 라벨. 한 번 들키면 다시는
        // 번역하지 않는다.
        bool m_assembling = false;
        std::string m_originalFont;
        std::string m_english;
        std::string m_korean;
    };

    // 한국어를 라벨에 올린다. 글꼴 교체까지 여기서 끝낸다.
    void applyKorean(std::string const& english, std::string const& korean, bool needUpdateLabel) {
        auto const& translator = kopatch::Translator::get();

        if (translator.ownFont() && !m_fields->m_usingOwnFont) {
            // 어느 글꼴을 쓰고 있었는지는 바꾸기 전에 봐야 한다. 바꾸고 나면
            // 우리 글꼴로 덮여서 원래 무엇이었는지 알 수 없다.
            std::string const current = m_sFntFile;

            m_fields->m_swappingFont = true;
            this->setFntFile(
                kopatch::ownFont(translator.pixelFont(), kopatch::wantsGold(current)).c_str());
            m_fields->m_swappingFont = false;
            m_fields->m_usingOwnFont = true;
            m_fields->m_originalFont = current;
        }

        m_fields->m_english = english;
        m_fields->m_korean = korean;
        CCLabelBMFont::setString(korean.c_str(), needUpdateLabel);
    }

    // 우리가 바꿔 놓은 글자 뒤에 영어가 덧붙고 있다. 이 라벨은 문장이 아니라
    // 조각을 쌓아 만드는 중이었다는 뜻이므로, 영어로 되돌리고 손을 뗀다.
    void undoKorean(char const* text, bool needUpdateLabel) {
        std::string restored = m_fields->m_english;
        restored += text + m_fields->m_korean.size();

        m_fields->m_assembling = true;
        m_fields->m_english.clear();
        m_fields->m_korean.clear();

        if (m_fields->m_usingOwnFont && !m_fields->m_originalFont.empty()) {
            m_fields->m_swappingFont = true;
            this->setFntFile(m_fields->m_originalFont.c_str());
            m_fields->m_swappingFont = false;
            m_fields->m_usingOwnFont = false;
        }

        CCLabelBMFont::setString(restored.c_str(), needUpdateLabel);
    }

    void setString(char const* text, bool needUpdateLabel) {
        auto const& translator = kopatch::Translator::get();

        // 여러 줄짜리 글은 MultilineBitmapFont 가 조각내어 이리로 보낸다.
        // 조각은 문장이 아니라 토막이라 표에 걸리면 안 된다. 그쪽은 쪼개지기
        // 전에 통째로 번역하므로 여기서는 손대지 않고 지나 보낸다.
        if (!text || m_fields->m_swappingFont || m_fields->m_assembling
            || !translator.enabled() || kopatch::splittingText()) {
            CCLabelBMFont::setString(text, needUpdateLabel);
            return;
        }

        // Geode 의 TextRenderer 는 라벨에 단어를 하나씩, 안 들어가면 글자를
        // 하나씩 덧붙여 가며 폭을 잰다. 붙일 때마다 라벨에 **지금 적힌 글자**를
        // 읽어서 거기에 잇는다. 그래서 우리가 첫 조각을 한국어로 바꿔 버리면
        // 그 뒤로 영어가 계속 그 위에 붙는다. "No" 를 "아니오" 로 바꾼 뒤
        // 글자가 하나씩 붙어 "아니오rmal" 이 되고, 단어가 붙어
        // "아니오 more clicking" 이 된다.
        //
        // 덧붙는 것이 보이면 우리가 문장이 아니라 조각을 번역한 것이다.
        // 되돌리고, 이 라벨에서는 손을 뗀다. 뒤에 붙는 것이 영어일 때만
        // 그렇게 본다. 한국어가 붙는 것은 게임이 라벨을 새로 쓴 것이다.
        std::string_view const incoming = text;
        if (!m_fields->m_korean.empty() && incoming.size() > m_fields->m_korean.size()
            && incoming.starts_with(m_fields->m_korean)
            && hasAsciiLetter(incoming.substr(m_fields->m_korean.size()))) {
            this->undoKorean(text, needUpdateLabel);
            return;
        }

        m_fields->m_english.clear();
        m_fields->m_korean.clear();

        // 이미 한글인 글자는 다른 한국어 패치가 먼저 바꿔 놓은 것이다. 거기에
        // 또 손을 대면 두 패치가 같은 라벨을 두고 서로 밀어내게 된다.
        if (kopatch::containsHangul(text)) {
            CCLabelBMFont::setString(text, needUpdateLabel);
            return;
        }

        if (auto const entry = translator.translate(text)) {
            if (entry->korean) {
                this->applyKorean(text, entry->text, needUpdateLabel);
            }
            else {
                CCLabelBMFont::setString(entry->text.c_str(), needUpdateLabel);
            }
            return;
        }

        if (auto const* learned = kopatch::gemini::find(text)) {
            this->applyKorean(text, *learned, needUpdateLabel);
            return;
        }

        // 표에도 없고 배운 적도 없다. 남은 할 일로 적어 둔다.
        kopatch::collector::note(text);

        // 답은 나중에 오므로 일단 원문을 그대로 띄우고, 도착하면 그때 바꿔 끼운다.
        if (kopatch::gemini::enabled() && kopatch::gemini::worthAsking(text)) {
            kopatch::gemini::request(
                std::string(text),
                [label = Ref<CCLabelBMFont>(this), source = std::string(text)](
                    std::string const& korean
                ) {
                    if (!label) {
                        return;
                    }
                    // 기다리는 사이 다른 글자로 바뀌었을 수 있다. 그때는 건드리지 않는다.
                    if (std::string_view(label->m_sInitialStringUTF8) != source) {
                        return;
                    }
                    static_cast<KoreanLabel*>(label.data())->applyKorean(source, korean, true);
                }
            );
        }

        CCLabelBMFont::setString(text, needUpdateLabel);
    }
};
