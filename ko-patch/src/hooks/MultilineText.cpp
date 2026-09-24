#include <Geode/Geode.hpp>
#include <Geode/modify/MultilineBitmapFont.hpp>

#include "KoreanFont.hpp"
#include "Translator.hpp"

using namespace geode::prelude;

namespace {
    // 깃발을 들었으면 반드시 내려놓게 한다. 중간에 빠져나가도 라벨 훅이
    // 잠든 채로 남지 않는다.
    struct SplitGuard {
        SplitGuard() { kopatch::setSplittingText(true); }
        ~SplitGuard() { kopatch::setSplittingText(false); }
    };
}

// 여러 줄짜리 글은 라벨 하나가 아니다. MultilineBitmapFont 가 문장을 조각내어
// 라벨 여러 개에 나눠 담고, 그 조각이 저마다 CCLabelBMFont::setString 을
// 지나간다. 조각을 번역하면 문장이 아니라 글자 토막이 표에 걸린다.
// "Normal" 이 통째로 오면 "일반" 이 되지만, "No" 만 떨어져 오면 표의 "No" 에
// 걸려 "아니오" 가 되고 남은 "rmal mode" 는 영어로 남는다. 그래서 화면에
// "아니오rmal mode" 가 나왔다. 종이를 찢어 놓고 조각마다 번역한 셈이다.
//
// 그래서 여기, 아직 한 문장일 때 바꾼다. 쪼개는 동안에는 라벨 훅을 재운다.
class $modify(KoreanMultiline, MultilineBitmapFont) {
    bool initWithFont(
        char const* font, gd::string text, float scale, float width,
        cocos2d::CCPoint anchor, int height, bool disableColor
    ) {
        auto const& translator = kopatch::Translator::get();

        if (!translator.enabled() || kopatch::splittingText()) {
            return MultilineBitmapFont::initWithFont(
                font, text, scale, width, anchor, height, disableColor);
        }

        std::string const source(text.c_str(), text.size());
        char const* useFont = font;
        std::string fontPath;

        // 이미 한글인 글은 다른 한국어 패치가 먼저 바꿔 놓은 것이다.
        if (!kopatch::containsHangul(source)) {
            if (auto const entry = translator.translate(source)) {
                text = entry->text;
                if (entry->korean && translator.ownFont()) {
                    fontPath = kopatch::ownFont(
                        translator.pixelFont(), kopatch::wantsGold(font ? font : ""));
                    useFont = fontPath.c_str();
                }
            }
        }

        SplitGuard const guard;
        return MultilineBitmapFont::initWithFont(
            useFont, text, scale, width, anchor, height, disableColor);
    }
};
