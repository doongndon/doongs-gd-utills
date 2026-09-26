#include <Geode/Geode.hpp>
#include <Geode/modify/MultilineBitmapFont.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
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
    // 글꼴 한 줄의 높이. 배율을 맞추는 데 쓴다.
    int lineHeight(char const* font) {
        if (!font) return 0;
        auto* probe = CCLabelBMFont::create(" ", font);
        if (!probe || !probe->getConfiguration()) return 0;
        return probe->getConfiguration()->m_nCommonHeight;
    }

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

    // 화면에 나오는 여러 줄 글 가운데는 줄을 하나씩 이어 붙여 만든 것이
    // 많다. BetterInfo 의 레벨 기록처럼 어떤 줄은 있고 어떤 줄은 없는 글은
    // 통째로는 결코 표에 걸리지 않는다. 그럴 때는 줄마다 따로 찾는다.
    struct ByLine {
        std::string text;
        bool korean = false;
    };

    std::optional<ByLine> translateByLine(std::string const& text) {
        if (text.find('\n') == std::string::npos) return std::nullopt;

        auto const& translator = kopatch::Translator::get();
        ByLine out;
        bool any = false;

        std::size_t start = 0;
        while (true) {
            std::size_t const end = std::min(text.find('\n', start), text.size());
            std::string_view const line(text.data() + start, end - start);

            if (auto const entry = translator.translate(line)) {
                out.text += entry->text;
                any = true;
                if (entry->korean) out.korean = true;
            }
            else {
                out.text.append(line);
                if (!line.empty()) kopatch::collector::note(line);
            }

            if (end >= text.size()) break;
            out.text += '\n';
            start = end + 1;
        }

        return any ? std::optional<ByLine>(std::move(out)) : std::nullopt;
    }

    // GD 는 한 줄의 너비도 바이트로 잰다. 한글 줄은 거의 0 으로 셈해지므로,
    // 그 숫자로 줄을 놓으면 가운데가 맞지 않고 오른쪽으로 밀린다. 글자를 다
    // 그린 뒤에 줄마다 제 실제 너비를 보고 다시 놓는다.
    //
    // 줄의 앵커를 정렬 방향에 맞추고 같은 축 위에 세우면, 가운데 정렬이든
    // 왼쪽 정렬이든 한 줄로 끝난다.
    void realign(cocos2d::CCNode* node, float anchorX) {
        if (!node) return;
        auto* children = node->getChildren();
        if (!children) return;

        std::vector<CCLabelBMFont*> lines;
        for (auto* child : CCArrayExt<CCNode*>(children)) {
            if (auto* line = typeinfo_cast<CCLabelBMFont*>(child)) lines.push_back(line);
        }
        if (lines.empty()) return;

        auto const widthOf = [](CCLabelBMFont* line) {
            return line->getContentSize().width * line->getScaleX();
        };
        auto const edgeOf = [&widthOf](CCLabelBMFont* line, float at) {
            return line->getPositionX() + (at - line->getAnchorPoint().x) * widthOf(line);
        };

        // 1) 줄끼리 맞춘다. 가장 긴 줄은 GD 가 그나마 바르게 놓았을 줄이니
        //    그 줄을 기준으로 나머지를 끌어다 맞춘다.
        if (lines.size() >= 2) {
            CCLabelBMFont* widest = lines.front();
            float widestSize = 0.f;
            for (auto* line : lines) {
                float const size = widthOf(line);
                if (size > widestSize) {
                    widestSize = size;
                    widest = line;
                }
            }
            float const target = edgeOf(widest, anchorX);
            for (auto* line : lines) {
                if (line == widest) continue;
                line->setPositionX(line->getPositionX() + (target - edgeOf(line, anchorX)));
            }
        }

        // 2) 덩어리 전체를 마디 안 제자리에 앉힌다.
        //
        //    GD 는 줄을 "줄바꿈 한계 너비" 의 한가운데에 놓으면서, 정작 마디의
        //    크기는 실제 글 너비로 잡는다. 영어는 한계까지 꽉 채워 쓰니 두 수가
        //    거의 같아 티가 안 나지만, 줄을 우리가 미리 나눈 한국어는 한계보다
        //    좁을 때가 많다. 그러면 남는 폭의 절반만큼 오른쪽으로 밀린다.
        //    창 지름이 아니라 종이 지름을 재서 가운데를 잡은 셈이다.
        //
        //    그래서 줄들이 실제로 차지한 자리를 우리가 재서, 마디의 크기를
        //    그 값으로 고치고 줄들을 0 에서 시작하게 옮긴다. 그러면 마디를
        //    붙인 쪽이 잡아 준 자리와 anchor 가 제대로 맞아떨어진다.
        //    이미 바르게 놓인 글은 잴 값이 같으므로 아무것도 달라지지 않는다.
        float left = edgeOf(lines.front(), 0.f);
        float right = left + widthOf(lines.front());
        for (auto* line : lines) {
            float const l = edgeOf(line, 0.f);
            left = std::min(left, l);
            right = std::max(right, l + widthOf(line));
        }

        float const span = right - left;
        if (span <= 0.f) return;

        auto size = node->getContentSize();
        if (std::fabs(size.width - span) > 0.5f || std::fabs(left) > 0.5f) {
            for (auto* line : lines) {
                line->setPositionX(line->getPositionX() - left);
            }
            size.width = span;
            node->setContentSize(size);
        }
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
            bool korean = false;

            if (auto const entry = translator.translate(source)) {
                source = entry->text;
                korean = entry->korean;
            }
            else if (auto const byLine = translateByLine(source)) {
                source = byLine->text;
                korean = byLine->korean;
            }
            else {
                kopatch::collector::note(source);
            }

            if (korean && translator.ownFont()) {
                fontPath = kopatch::ownFont(
                    translator.pixelFont(), kopatch::wantsGold(font ? font : ""));
                useFont = fontPath.c_str();

                // 글꼴이 바뀌면 한 줄의 높이도 바뀐다. 같은 배율로 두면
                // 글씨가 작아 보이므로, 높이가 달라진 만큼 배율을 되돌린다.
                SplitGuard const guard;
                int const before = lineHeight(font);
                int const after = lineHeight(useFont);
                if (before > 0 && after > 0) {
                    scale *= static_cast<float>(before) / static_cast<float>(after);
                }
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

        if (kopatch::containsHangul(source)) {
            realign(this, anchor.x);
        }

        if (paint) {
            kopatch::colortags::apply(this, parsed.plain, parsed.spans);
        }
        return true;
    }
};
