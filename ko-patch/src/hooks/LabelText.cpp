#include <Geode/Geode.hpp>
#include <Geode/modify/CCLabelBMFont.hpp>

#include <algorithm>

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
        // 글꼴을 바꾸며 곱해 둔 배율. 되돌릴 때 그만큼 나눈다.
        float m_fontScale = 1.f;
        // 조각을 이어 붙여 문장을 만드는 중인 라벨. 한 번 들키면 다시는
        // 번역하지 않는다.
        bool m_assembling = false;
        std::string m_originalFont;
        std::string m_english;
        std::string m_korean;
        // 왼쪽 끝을 지키려고 오른쪽으로 민 적이 있다면 그 전 자리를 기억해
        // 둔다. 같은 라벨을 재활용해 다시 영어로 얹었다가 한글로 바꿔치는
        // 목록 칸 같은 자리에서, 밀어 둔 것 위에 또 밀어 자리가 자꾸
        // 벌어지는 것을 막는다.
        bool m_hasBaselineX = false;
        float m_baselineX = 0.f;
    };

    // 한국어를 라벨에 올린다. 글꼴 교체까지 여기서 끝낸다.
    void applyKorean(std::string const& english, std::string const& korean, bool needUpdateLabel) {
        auto const& translator = kopatch::Translator::get();

        // 영어가 차지하던 너비. 단추는 영어에 맞춰 만들어졌으므로 이것이
        // 우리에게 허락된 자리다.
        float english_width = 0.f;

        // 자리를 재려면 먼저 영어를 그 글꼴로 올려 놓아야 한다.
        CCLabelBMFont::setString(english.c_str(), needUpdateLabel);

        if (translator.ownFont() && !m_fields->m_usingOwnFont) {
            // 어느 글꼴을 쓰고 있었는지는 바꾸기 전에 봐야 한다. 바꾸고 나면
            // 우리 글꼴로 덮여서 원래 무엇이었는지 알 수 없다.
            std::string const current = m_sFntFile;

            english_width = this->getContentSize().width;

            // 글꼴마다 한 줄의 높이가 다르다. 우리 글꼴이 게임 글꼴보다 낮으면
            // 같은 배율로 그려도 글씨가 작아 보인다. 자를 바꾸면 눈금도 바꿔야
            // 길이가 그대로이듯, 글꼴을 바꾸면 배율도 그만큼 되돌려 놓는다.
            int const before = this->getConfiguration()
                ? this->getConfiguration()->m_nCommonHeight : 0;

            m_fields->m_swappingFont = true;
            this->setFntFile(
                kopatch::ownFont(translator.pixelFont(), kopatch::wantsGold(current)).c_str());
            m_fields->m_swappingFont = false;
            m_fields->m_usingOwnFont = true;
            m_fields->m_originalFont = current;

            int const after = this->getConfiguration()
                ? this->getConfiguration()->m_nCommonHeight : 0;
            if (before > 0 && after > 0 && before != after) {
                float const fit = static_cast<float>(before) / static_cast<float>(after);
                m_fields->m_fontScale = fit;
                this->setScale(this->getScale() * fit);
            }
        }

        m_fields->m_english = english;
        m_fields->m_korean = korean;
        CCLabelBMFont::setString(korean.c_str(), needUpdateLabel);

        // 한글은 같은 뜻을 더 넓게 적는 일이 많다. 단추는 영어에 맞춰 잘려
        // 있으므로, 넘치면 줄여서 그 안에 앉힌다.
        //
        // 다만 끝까지 줄이지는 않는다. "Tags" 두 글자가 "갈래" 가 되면 한글이
        // 더 넓어 배율이 뚝 떨어지는데, 그 라벨은 사실 넘칠 자리도 아니었다.
        // 밖으로 조금 나가는 것보다 못 읽을 만큼 작아지는 것이 나쁘다.
        // 그래서 눈에 띄게 넘칠 때만 손대고, 줄여도 4분의 3까지만 줄인다.
        // 다만 얼마나 넘치느냐에 따라 다르게 다룬다. 조금 넘치는 것은 단추
        // 테두리를 살짝 벗어나는 정도라 읽는 데 지장이 없지만, 배가 넘게
        // 넘치는 것은 옆 화면까지 밀고 들어간다. 앞은 4분의 3까지만 줄이고,
        // 뒤는 절반까지 줄여서라도 제자리에 앉힌다.
        constexpr float ALLOW = 1.05f;  // 이만큼까지는 넘쳐도 둔다
        constexpr float MUCH  = 1.50f;  // 이보다 넘치면 많이 넘치는 것이다
        if (english_width > 1.f) {
            float const now = this->getContentSize().width * m_fields->m_fontScale;
            if (now > english_width * ALLOW) {
                float const floor = now > english_width * MUCH ? 0.5f : 0.75f;
                float const shrink = std::max(floor, english_width / now);
                this->setScale(this->getScale() * shrink);
                m_fields->m_fontScale *= shrink;
            }
        }

        // 라벨이 가운데나 오른쪽에 앵커를 두고 있으면, 넓어진 한글이 왼쪽으로도
        // 자란다. 업적 목록의 자물쇠·체크 그림 옆 칸이 그렇다 - 그림은 GD 가
        // 제자리에 두고, 설명 글만 한글이 되며 넓어져 왼쪽 끝이 그림 밑으로
        // 파고든다. 앵커가 0(왼쪽)이면 글은 오른쪽으로만 자라니 손댈 일이
        // 없지만, 그보다 크면 자란 만큼 왼쪽 끝이 밀려난 것이니 그 자란
        // 만큼만 오른쪽으로 되민다. 글의 한가운데는 그대로 두고 왼쪽 끝만
        // 원래 자리를 지키게 하는 셈이다.
        float const anchorX = this->getAnchorPoint().x;
        if (english_width > 1.f && anchorX > 0.001f) {
            if (!m_fields->m_hasBaselineX) {
                m_fields->m_baselineX = this->getPositionX();
                m_fields->m_hasBaselineX = true;
            }
            else {
                // 전에 밀어 둔 자리를 되돌리고 이번 값으로 다시 잰다.
                this->setPositionX(m_fields->m_baselineX);
            }
            float const now = this->getContentSize().width * m_fields->m_fontScale;
            float const grew = (now - english_width) * anchorX;
            if (grew > 0.5f) {
                this->setPositionX(m_fields->m_baselineX + grew);
            }
        }
    }

    // 우리가 바꿔 놓은 글자 뒤에 영어가 덧붙고 있다. 이 라벨은 문장이 아니라
    // 조각을 쌓아 만드는 중이었다는 뜻이므로, 영어로 되돌리고 손을 뗀다.
    void undoKorean(char const* text, bool needUpdateLabel) {
        std::string restored = m_fields->m_english;
        restored += text + m_fields->m_korean.size();

        m_fields->m_english.clear();
        m_fields->m_korean.clear();

        if (m_fields->m_usingOwnFont && !m_fields->m_originalFont.empty()) {
            m_fields->m_swappingFont = true;
            this->setFntFile(m_fields->m_originalFont.c_str());
            m_fields->m_swappingFont = false;
            m_fields->m_usingOwnFont = false;
            if (m_fields->m_fontScale > 0.f && m_fields->m_fontScale != 1.f) {
                this->setScale(this->getScale() / m_fields->m_fontScale);
                m_fields->m_fontScale = 1.f;
            }
        }

        // 덧붙은 결과가 그 자체로 온전한 문장일 수도 있다. Tinker 와
        // BetterEdit 은 편집기의 물체 수 라벨을 **읽어서** 그 뒤에
        // " | LDM: 0 (0%)" 을 이어 붙인다. 우리가 앞부분을 한국어로 바꿔 둔
        // 뒤였으므로 한국어 뒤에 영어가 붙은 꼴이 되고, 조각을 쌓는 것과
        // 구별되지 않는다. 다만 이어 붙은 그 문장이 표에 통째로 있다.
        //
        // 그래서 되돌린 문장을 한 번 찾아본다. 찾으면 그것이 문장이었다는
        // 뜻이니 다시 한국어로 올리고, 못 찾으면 그제서야 조각으로 보고
        // 이 라벨에서 손을 뗀다.
        if (!m_fields->m_assembling) {
            if (auto const entry = kopatch::Translator::get().translate(restored)) {
                if (entry->korean) {
                    this->applyKorean(restored, entry->text, needUpdateLabel);
                }
                else {
                    CCLabelBMFont::setString(entry->text.c_str(), needUpdateLabel);
                }
                return;
            }
        }

        m_fields->m_assembling = true;
        CCLabelBMFont::setString(restored.c_str(), needUpdateLabel);
    }

    // getString 은 후킹하면 안 된다. CCLabelBMFont 는 CCLabelProtocol 도 함께
    // 물려받으므로 getString 에는 this 를 옮겨 주는 8 바이트짜리 썽크가 따로
    // 있는데, 그 8 바이트 안에는 훅이 들어갈 자리가 없다. 밀어 넣으면 썽크
    // 뒤의 남의 코드를 덮어쓰고, 게임은 그 자리를 명령으로 읽다 죽는다.
    // (SIGILL, non-virtual thunk to CCLabelBMFont::getString)
    //
    // 끝 화면 이름 어긋남은 다른 길로 고친다. EndLevel.cpp 를 보라.

    void setString(char const* text, bool needUpdateLabel) {
        auto const& translator = kopatch::Translator::get();

        // createBatched 로 만든 라벨은 제 텍스처 아틀라스가 없다. 다른 곳에 묶여
        // 그려지기 때문이다. 거기에 글꼴을 갈아 끼우려 하면 없는 아틀라스를 만지다
        // 게임이 터진다. 레벨 안의 글자 오브젝트가 그렇게 만들어지는데, 그것은
        // 어차피 레벨을 만든 사람이 쓴 글이라 번역할 것도 아니다.
        if (!this->getTextureAtlas()) {
            CCLabelBMFont::setString(text, needUpdateLabel);
            return;
        }

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
