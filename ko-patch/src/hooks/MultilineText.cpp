#include <Geode/Geode.hpp>
#include <Geode/modify/MultilineBitmapFont.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "Collector.hpp"
#include "ColorTags.hpp"
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

    // GD 가 줄을 나누는 stringWithMaxWidth 는 글을 바이트 단위로 훑으며 글꼴의
    // 글자표에서 너비를 찾는다. 한글 한 글자는 세 바이트이고 그 바이트값은
    // 글자표에 없으므로 너비가 0 으로 셈해진다. 그래서 한글 문장은 아무리 길어도
    // 한계에 걸리지 않고 한 줄로 이어져 상자 밖으로 흘러나간다.
    //
    // 자로 재지 못하는 자에게 종이를 맡기지 않는다. 줄은 우리가 나눠서 건넨다.
    // 라벨을 하나 만들어 실제로 재 보고, 한계를 넘기 직전에서 끊는다.
    std::string wrapToWidth(
        std::string const& text, char const* font, float scale, float maxWidth
    ) {
        // 픽셀 너비가 아닌 값이 들어오는 자리가 있을까 봐, 상자 하나도 담지
        // 못할 만큼 작은 한계는 믿지 않고 그대로 넘긴다.
        if (maxWidth < 40.f || scale <= 0.f || text.empty()) return text;

        auto* ruler = CCLabelBMFont::create(" ", font);
        if (!ruler) return text;

        // 재는 동안에는 라벨 훅이 끼어들지 않아야 한다.
        SplitGuard const guard;

        auto const measure = [&](std::string const& piece) {
            ruler->setString(piece.c_str());
            return ruler->getContentSize().width * scale;
        };

        // UTF-8 한 글자의 바이트 수. 낱말 하나가 통째로 한계를 넘을 때 쓴다.
        auto const charLength = [](std::string const& s, std::size_t i) -> std::size_t {
            unsigned char const c = static_cast<unsigned char>(s[i]);
            std::size_t const len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xe ? 3 : 4;
            return std::min(len, s.size() - i);
        };

        std::string out;
        std::size_t lineStart = 0;

        // 이미 들어 있는 줄바꿈은 그대로 지킨다.
        while (lineStart <= text.size()) {
            std::size_t const lineEnd = std::min(text.find('\n', lineStart), text.size());
            std::string_view const paragraph(text.data() + lineStart, lineEnd - lineStart);

            std::string line;
            std::size_t word = 0;
            while (word < paragraph.size()) {
                std::size_t const space = std::min(paragraph.find(' ', word), paragraph.size());
                std::string const piece(paragraph.substr(word, space - word));

                std::string candidate = line.empty() ? piece : line + " " + piece;
                if (!line.empty() && measure(candidate) > maxWidth) {
                    out += line;
                    out += '\n';
                    line = piece;
                    candidate = piece;
                }
                else {
                    line = candidate;
                }

                // 낱말 하나가 한 줄보다 길면 글자 단위로 끊는다.
                while (measure(line) > maxWidth) {
                    std::string kept;
                    for (std::size_t i = 0; i < line.size();) {
                        std::size_t const step = charLength(line, i);
                        std::string const next = kept + line.substr(i, step);
                        if (!kept.empty() && measure(next) > maxWidth) break;
                        kept = next;
                        i += step;
                    }
                    if (kept.size() >= line.size()) break;
                    out += kept;
                    out += '\n';
                    line.erase(0, kept.size());
                }

                word = space + 1;
            }

            out += line;
            if (lineEnd >= text.size()) break;
            out += '\n';
            lineStart = lineEnd + 1;
        }

        return out;
    }
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

        std::string source(text.c_str(), text.size());
        char const* useFont = font;
        std::string fontPath;

        // 이미 한글인 글은 다른 한국어 패치가 먼저 바꿔 놓은 것이다.
        if (!kopatch::containsHangul(source)) {
            if (auto const entry = translator.translate(source)) {
                source = entry->text;
                if (entry->korean && translator.ownFont()) {
                    fontPath = kopatch::ownFont(
                        translator.pixelFont(), kopatch::wantsGold(font ? font : ""));
                    useFont = fontPath.c_str();
                }
            }
            else {
                kopatch::collector::note(source);
            }
        }

        // GD 는 <cg>...</c> 가 덮는 범위를 바이트로 세어 두었다가 글자 번호로
        // 쓴다. 영어는 한 글자가 한 바이트라 두 수가 같지만, 한글은 세 바이트라
        // 색이 세 배 길게 번진다. "상자" 여섯 바이트가 "상자를 10" 여섯 글자를
        // 칠해 버린 것이 그것이다.
        //
        // 그래서 색표를 우리가 떼어 내고, 글자를 다 그린 뒤에 우리 손으로
        // 칠한다. 자가 잘못됐으면 자를 빼앗는 편이 낫다.
        kopatch::colortags::Parsed parsed;
        bool paint = false;
        if (!disableColor && kopatch::containsHangul(source)
            && kopatch::colortags::hasTags(source)) {
            parsed = kopatch::colortags::parse(source);
            source = parsed.plain;
            paint = !parsed.spans.empty();
        }

        if (kopatch::containsHangul(source)) {
            source = wrapToWidth(source, useFont, scale, width);
        }

        text = source;

        SplitGuard const guard;
        if (!MultilineBitmapFont::initWithFont(
                useFont, text, scale, width, anchor, height, disableColor)) {
            return false;
        }

        if (paint) {
            kopatch::colortags::apply(this, parsed.plain, parsed.spans);
        }
        return true;
    }
};
