#include <Geode/Geode.hpp>
#include <Geode/modify/CCLabelBMFont.hpp>
#include <Geode/binding/AchievementBar.hpp>
#include <Geode/binding/AchievementCell.hpp>
#include <Geode/binding/MultilineBitmapFont.hpp>
#include <Geode/binding/TextArea.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

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

    bool isAchievementCellLabel(CCLabelBMFont* label) {
        for (auto* node = static_cast<CCNode*>(label); node; node = node->getParent()) {
            if (typeinfo_cast<AchievementCell*>(node)) {
                return true;
            }
        }
        return false;
    }

    bool isAchievementBarLabel(CCLabelBMFont* label) {
        for (auto* node = static_cast<CCNode*>(label); node; node = node->getParent()) {
            if (typeinfo_cast<AchievementBar*>(node)) {
                return true;
            }
        }
        return false;
    }

    bool isAchievementCondition(std::string_view text) {
        bool const hasCompletionWord =
            text.starts_with("Complete ") || text.starts_with("Completed ");
        bool const hasMode =
            text.find(" in Normal mode") != std::string_view::npos
            || text.find(" in Practice mode") != std::string_view::npos;
        return hasCompletionWord && hasMode;
    }

    struct WorldBounds {
        float left;
        float right;
        float bottom;
        float top;
    };

    WorldBounds worldBounds(CCNode* node) {
        auto const size = node->getContentSize();
        auto const anchor = node->getAnchorPoint();
        auto const bottomLeft = node->convertToWorldSpace({
            -anchor.x * size.width,
            -anchor.y * size.height
        });
        auto const topRight = node->convertToWorldSpace({
            (1.f - anchor.x) * size.width,
            (1.f - anchor.y) * size.height
        });
        return {
            std::min(bottomLeft.x, topRight.x),
            std::max(bottomLeft.x, topRight.x),
            std::min(bottomLeft.y, topRight.y),
            std::max(bottomLeft.y, topRight.y)
        };
    }

    // 글자는 아이콘이 아니다. 라벨의 글자 하나하나도 CCSprite 라서 그대로
    // 두면, 라벨이 제 첫 글자를 아이콘으로 알고 피하느라 오른쪽으로 밀려난다.
    // 글 상자(TextArea)도 CCSprite 를 물려받았으니 함께 뺀다.
    bool isText(CCNode* node) {
        return typeinfo_cast<CCLabelBMFont*>(node)
            || typeinfo_cast<TextArea*>(node)
            || typeinfo_cast<MultilineBitmapFont*>(node);
    }

    void collectAchievementSprites(CCNode* node, std::vector<CCSprite*>& sprites) {
        if (isText(node)) return;
        if (auto* sprite = typeinfo_cast<CCSprite*>(node)) {
            sprites.push_back(sprite);
        }

        auto* children = node->getChildren();
        if (!children) return;
        for (auto* child : CCArrayExt<CCNode*>(children)) {
            collectAchievementSprites(child, sprites);
        }
    }

    void collectAchievementNodes(CCNode* node, std::vector<CCNode*>& nodes) {
        if (node && isText(node)) return;
        if (node) {
            nodes.push_back(node);
        }

        auto* children = node ? node->getChildren() : nullptr;
        if (!children) return;
        for (auto* child : CCArrayExt<CCNode*>(children)) {
            collectAchievementNodes(child, nodes);
        }
    }

    float achievementIconOverlapShift(CCLabelBMFont* label) {
        CCNode* container = nullptr;
        for (auto* node = label->getParent(); node; node = node->getParent()) {
            if (typeinfo_cast<AchievementCell*>(node)) {
                container = node;
                break;
            }
        }
        if (!label->isVisible()) return 0.f;

        auto const text = worldBounds(label);

        // 아이콘이 글의 왼쪽 끝을 덮고 있을 때만 민다. 위아래로 겹치는지만 보고
        // 옆은 보지 않으면, 글 오른쪽에 떨어져 있는 완료 표시나 한가운데의
        // 자물쇠까지 장애물이 되어 글을 그 너머 오른쪽 끝까지 밀어 버린다.
        auto const coversLeftEnd = [&text](WorldBounds const& icon) {
            if (text.top <= icon.bottom || text.bottom >= icon.top) return false;
            return icon.left <= text.left + 1.f && icon.right > text.left;
        };

        auto requiredShiftIn = [&](CCNode* node) {
            if (!node) return 0.f;

            std::vector<CCSprite*> sprites;
            collectAchievementSprites(node, sprites);
            float requiredShift = 0.f;
            for (auto* sprite : sprites) {
                if (!sprite->isVisible()) continue;

                auto const icon = worldBounds(sprite);
                auto const iconWidth = icon.right - icon.left;
                auto const iconHeight = icon.top - icon.bottom;

                // AchievementCell 안의 작은 스프라이트만 아이콘 후보로 본다.
                // 배경 판넬이나 테두리까지 장애물로 취급하면 글자를 화면
                // 오른쪽 끝까지 밀어 버린다.
                if (iconWidth < 8.f || iconWidth > 120.f
                    || iconHeight < 8.f || iconHeight > 120.f) {
                    continue;
                }
                if (!coversLeftEnd(icon)) continue;

                requiredShift = std::max(requiredShift, icon.right - text.left + 3.f);
            }

            // 일부 GD 빌드에서는 업적 아이콘이 일반 CCSprite가 아닌 래퍼
            // CCNode 안에 들어 있어 typeinfo_cast<CCSprite>로 잡히지 않는다.
            // 스프라이트를 하나 찾았더라도 래퍼 안의 실제 아이콘을 계속 확인한다.
            std::vector<CCNode*> nodes;
            collectAchievementNodes(node, nodes);
            for (auto* candidate : nodes) {
                if (candidate == label || !candidate->isVisible()) continue;
                if (typeinfo_cast<CCSprite*>(candidate)
                    || typeinfo_cast<CCLabelBMFont*>(candidate)
                    || typeinfo_cast<TextArea*>(candidate)) {
                    continue;
                }

                auto const icon = worldBounds(candidate);
                auto const iconWidth = icon.right - icon.left;
                auto const iconHeight = icon.top - icon.bottom;
                if (iconWidth < 8.f || iconWidth > 120.f
                    || iconHeight < 8.f || iconHeight > 120.f) {
                    continue;
                }
                if (!coversLeftEnd(icon)) continue;

                requiredShift = std::max(requiredShift, icon.right - text.left + 3.f);
            }
            return requiredShift;
        };

        float requiredShift = requiredShiftIn(container);
        // 게임 버전에 따라 AchievementCell 타입이 라벨의 조상으로 노출되지
        // 않는 경우가 있다. 조건 문장인 것이 이미 확인된 호출에서는 가까운
        // 부모들도 살펴 아이콘을 놓치지 않는다.
        if (requiredShift <= 0.f) {
            int depth = 0;
            for (auto* node = label->getParent(); node && depth < 24;
                 node = node->getParent(), ++depth) {
                requiredShift = requiredShiftIn(node);
                if (requiredShift > 0.f) break;
            }
        }
        if (requiredShift <= 0.f) return 0.f;

        auto* parent = label->getParent();
        if (!parent) return 0.f;
        auto const worldPosition = parent->convertToWorldSpace(label->getPosition());
        auto const localPosition = parent->convertToNodeSpace({
            worldPosition.x + requiredShift,
            worldPosition.y
        });
        return localPosition.x - label->getPositionX();
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
        // 글꼴 높이를 맞추거나 넘침을 줄이며 곱해 둔 전체 배율.
        // 원래 글꼴로 되돌릴 때 그만큼 나눈다.
        float m_fontScale = 1.f;
        // 글꼴 높이를 맞춘 배율과 별개로, 이전 업적 행의 넘침을 줄인
        // 배율. AchievementCell 은 라벨을 재활용하므로 다음 행을 그리기
        // 전에 이것만 되돌려야 한다.
        float m_widthScale = 1.f;
        // 조각을 이어 붙여 문장을 만드는 중인 라벨. 한 번 들키면 다시는
        // 번역하지 않는다.
        bool m_assembling = false;
        std::string m_originalFont;
        std::string m_english;
        std::string m_korean;
        // 우리가 마지막으로 더한 X 이동량. 업적 행은 라벨을 재활용하므로
        // 이전 행의 절대 위치를 기억하면 안 된다. 다음 행에서는 게임이 새로
        // 잡은 위치에서 이 이동량만 먼저 빼고, 현재 위치를 새 기준으로 쓴다.
        float m_positionCorrection = 0.f;
        float m_lastPatchedX = 0.f;
        bool m_hasPatchedPosition = false;
        // 한국어를 올린 프레임. 조각을 덧붙여 문장을 쌓는 쪽은 한 프레임 안에서
        // 몰아서 붙인다. 다음 프레임에 더 긴 글이 오는 것은 게임이 라벨을 새로
        // 쓴 것이지 덧붙이는 것이 아니다.
        unsigned int m_koreanFrame = 0;
    };

    void restorePositionCorrection() {
        if (m_fields->m_positionCorrection == 0.f) return;
        float const current = this->getPositionX();
        float const patched = m_fields->m_lastPatchedX;
        float const unpatched = patched - m_fields->m_positionCorrection;
        if (m_fields->m_hasPatchedPosition && std::fabs(current - unpatched) <= 1.f) {
            // 게임이 우리의 이동을 이미 덮어쓴 상태다.
        }
        else if (!m_fields->m_hasPatchedPosition || std::fabs(current - patched) <= 1.f) {
            this->setPositionX(current - m_fields->m_positionCorrection);
        }
        m_fields->m_positionCorrection = 0.f;
        m_fields->m_hasPatchedPosition = false;
    }

    // 한국어를 라벨에 올린다. 글꼴 교체까지 여기서 끝낸다.
    void applyKorean(std::string const& english, std::string const& korean, bool needUpdateLabel) {
        auto const& translator = kopatch::Translator::get();

        // 비동기 번역처럼 setString 을 거치지 않고 바로 들어오는 경로도
        // 있다. 라벨을 재활용할 때는 우리가 전에 더한 이동만 되돌리고,
        // 게임이 이번 행에 설정한 현재 위치는 그대로 새 기준으로 삼는다.
        restorePositionCorrection();

        // 업적 목록은 몇 개의 라벨을 돌려 쓰며 모든 행을 그린다. 앞 행의
        // 한국어가 너무 길어서 줄어든 배율을 그대로 두면, 뒤 행도 작게
        // 그려지고 폭을 다시 잴 때 기준까지 틀어진다. 글꼴 자체의 높이를
        // 맞춘 배율(m_fontScale)은 유지하고, 넘침 때문에 줄인 부분만
        // 원상 복구한다.
        if (m_fields->m_widthScale > 0.f && m_fields->m_widthScale != 1.f) {
            this->setScale(this->getScale() / m_fields->m_widthScale);
            m_fields->m_fontScale /= m_fields->m_widthScale;
            m_fields->m_widthScale = 1.f;
        }
        // 영어가 차지하던 너비. 단추는 영어에 맞춰 만들어졌으므로 이것이
        // 우리에게 허락된 자리다.
        float english_width = 0.f;

        // 자리를 재려면 먼저 영어를 그 글꼴로 올려 놓아야 한다.
        CCLabelBMFont::setString(english.c_str(), needUpdateLabel);

        // 글꼴을 이미 패치 글꼴로 바꾼 라벨도 있다. 업적 목록처럼 라벨을
        // 재활용하는 화면에서는 아래의 글꼴 교체 분기를 건너뛸 수 있으므로,
        // 영어 폭은 글꼴 교체 여부와 관계없이 측정해야 한다. 이미 패치
        // 글꼴을 쓰는 경우에는 현재 렌더링 배율까지 반영한다.
        english_width = this->getContentSize().width * m_fields->m_fontScale;
        // 처음부터 한국어였던 글은 글꼴만 바꾸는 것이다. 게임 글꼴로 잰 너비는
        // 한글이 빠진 너비라 기준으로 삼을 수 없다.
        if (english == korean) english_width = 0.f;

        if (translator.ownFont() && !m_fields->m_usingOwnFont) {
            // 어느 글꼴을 쓰고 있었는지는 바꾸기 전에 봐야 한다. 바꾸고 나면
            // 우리 글꼴로 덮여서 원래 무엇이었는지 알 수 없다.
            std::string const current = m_sFntFile;


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
        m_fields->m_koreanFrame = CCDirector::sharedDirector()->getTotalFrames();
        CCLabelBMFont::setString(korean.c_str(), needUpdateLabel);

        // 화면에는 한국어를 그리되, 누가 이 라벨에게 "뭐라고 적혀 있니" 하고
        // 물으면(getString) 원래 영어로 답한다. 모드들은 라벨의 글자를 영어와
        // 견주어 자기 할 일을 정한다. Overcharged Levels 는 "Coming Soon!"
        // 라벨만 빼고 나머지 라벨을 옆으로 옮기는데, 번역된 글자로는 그 라벨을
        // 알아보지 못해 "곧 나옵니다!" 를 화면 왼쪽 밖으로 밀어냈다. NodeIDs,
        // NamedEditorGroups, textureldr, Tinker 도 같은 식으로 단추를 찾는다.
        // 간판은 한국어로 바꿔 달아도 주소는 그대로 두는 셈이다.
        //
        // 그려지는 글자는 따로 들고 있으므로(m_sInitialString) 화면은 한국어
        // 그대로다. 줄을 다시 맞출 때도 cocos 는 그쪽을 쓴다.
        if (needUpdateLabel) {
            m_sInitialStringUTF8 = english;
        }

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
        if (translator.ownFont() && english_width > 1.f) {
            float const now = this->getContentSize().width * m_fields->m_fontScale;
            if (now > english_width * ALLOW) {
                float const floor = now > english_width * MUCH ? 0.5f : 0.75f;
                float const shrink = std::max(floor, english_width / now);
                this->setScale(this->getScale() * shrink);
                m_fields->m_fontScale *= shrink;
                m_fields->m_widthScale *= shrink;
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
        // 이 보정은 업적 목록/알림의 조건 문장에만 적용한다. 모든 라벨에
        // 적용하면 가운데 정렬된 메뉴 제목까지 오른쪽으로 밀려 원래 자리에서
        // 벗어난다.
        bool const achievementCondition = isAchievementCondition(english);
        bool const achievementCell = isAchievementCellLabel(this);
        bool const achievementBar = isAchievementBarLabel(this);
        bool const shouldAlignAchievement =
            !achievementBar && (achievementCell || achievementCondition);
        // 넓어진 만큼 오른쪽으로 미는 것은 업적 목록 행 안에서만 한다. 조건
        // 문장이라는 것만으로 밀면, 가운데 정렬로 놓인 다른 모드의 조건 줄
        // (Achievements Reimagined 의 업적 칸) 이 가운데에서 벗어난다.
        if (shouldAlignAchievement && achievementCell
            && english_width > 1.f && anchorX > 0.001f) {
            float const now = this->getContentSize().width * m_fields->m_fontScale;
            float const grew = std::max(0.f, (now - english_width) * anchorX);
            if (grew > 0.5f) {
                this->setPositionX(this->getPositionX() + grew);
                m_fields->m_positionCorrection += grew;
                m_fields->m_lastPatchedX = this->getPositionX();
                m_fields->m_hasPatchedPosition = true;
            }
        }

        // 업적 화면의 배치는 글자 폭만 보고 정해지지 않는다. 아이콘이
        // 라벨의 왼쪽 영역 위에 그려지는 행도 있어서, 영어보다 넓어지지
        // 않은 문장도 아이콘 아래에 숨을 수 있다. 실제 스프라이트의
        // 오른쪽 경계를 보고 겹칠 때만 최소한으로 민다.
        if (shouldAlignAchievement) {
            float const iconShift = achievementIconOverlapShift(this);
            if (iconShift > 0.f) {
                this->setPositionX(this->getPositionX() + iconShift);
                m_fields->m_positionCorrection += iconShift;
                m_fields->m_lastPatchedX = this->getPositionX();
                m_fields->m_hasPatchedPosition = true;
            }
            if (achievementCell || achievementCondition) {
                this->scheduleOnce(
                    schedule_selector(KoreanLabel::recheckAchievementPosition), 0.f
                );
            }
        }
    }

    void recheckAchievementPosition(float) {
        bool const achievementCondition = isAchievementCondition(m_fields->m_english);
        if ((!isAchievementCellLabel(this) && !achievementCondition)
            || isAchievementBarLabel(this)) {
            return;
        }
        restorePositionCorrection();
        float const shift = achievementIconOverlapShift(this);
        if (shift <= 0.f) return;
        this->setPositionX(this->getPositionX() + shift);
        m_fields->m_positionCorrection = shift;
        m_fields->m_lastPatchedX = this->getPositionX();
        m_fields->m_hasPatchedPosition = true;
    }

    // 우리가 바꿔 놓은 글자 뒤에 영어가 덧붙고 있다. 이 라벨은 문장이 아니라
    // 조각을 쌓아 만드는 중이었다는 뜻이므로, 영어로 되돌리고 손을 뗀다.
    // restored 는 덧붙은 것까지 합친 영어 문장이다.
    void undoKorean(std::string restored, bool needUpdateLabel) {
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
            m_fields->m_widthScale = 1.f;
        }

        // 덧붙은 결과가 그 자체로 온전한 문장일 수도 있다. Tinker 와
        // BetterEdit 은 편집기의 물체 수 라벨을 **읽어서** 그 뒤에
        // " | LDM: 0 (0%)" 을 이어 붙인다. 겉보기로는 조각을 쌓는 것과
        // 구별되지 않지만, 이어 붙은 그 문장이 표에 통째로 있다.
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

        // 업적 목록은 같은 라벨을 여러 행에 돌려 쓴다. 이전 번역에서
        // 아이콘을 피하려고 더한 이동량이 새 행까지 따라가지 않게 한다.
        restorePositionCorrection();

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

        // 지금 그리는 레벨의 이름이다. 이름은 번역하지 않는다. KoreanFont.hpp 를 보라.
        if (kopatch::isLevelNameInScope(text)) {
            m_fields->m_english.clear();
            m_fields->m_korean.clear();
            CCLabelBMFont::setString(text, needUpdateLabel);
            return;
        }

        // Geode 의 TextRenderer 는 라벨에 단어를 하나씩, 안 들어가면 글자를
        // 하나씩 덧붙여 가며 폭을 잰다. 붙일 때마다 라벨에 **지금 적힌 글자**를
        // 읽어서 거기에 잇는다. 첫 조각 "No" 가 표에 걸려 "아니오" 가 되면,
        // 그 뒤로 "Nor", "Norm" 처럼 계속 덧붙는다.
        //
        // 덧붙는 것이 보이면 우리가 문장이 아니라 조각을 번역한 것이다.
        // 되돌리고, 이 라벨에서는 손을 뗀다. 뒤에 붙는 것이 영어일 때만
        // 그렇게 본다. 한국어가 붙는 것은 게임이 라벨을 새로 쓴 것이다.
        //
        // 라벨은 물으면 영어로 답하므로 덧붙는 앞부분은 보통 영어다. 그래도
        // 한국어를 따로 들고 있다가 거기에 잇는 쪽이 있을 수 있어 둘 다 본다.
        // 영어 쪽은 같은 프레임 안에서만 덧붙이기로 본다. 다음 프레임에
        // "Play" 가 "Play again" 이 되는 것은 게임이 새로 쓴 것이다.
        std::string_view const incoming = text;
        auto const grewFrom = [incoming](std::string const& base) {
            return !base.empty() && incoming.size() > base.size()
                && incoming.starts_with(base)
                && hasAsciiLetter(incoming.substr(base.size()));
        };
        // 처음부터 한국어여서 글꼴만 바꾼 라벨(m_english == m_korean)은 번역한 적이
        // 없으니 덧붙어도 되돌릴 것이 없다. 되돌리면 게임 글꼴로 돌아가 다시
        // 빈칸이 된다.
        if (!m_fields->m_korean.empty() && m_fields->m_korean != m_fields->m_english) {
            bool const sameFrame =
                m_fields->m_koreanFrame == CCDirector::sharedDirector()->getTotalFrames();
            if (sameFrame && grewFrom(m_fields->m_english)) {
                this->undoKorean(std::string(incoming), needUpdateLabel);
                return;
            }
            if (grewFrom(m_fields->m_korean)) {
                this->undoKorean(
                    m_fields->m_english + std::string(incoming.substr(m_fields->m_korean.size())),
                    needUpdateLabel);
                return;
            }
        }

        m_fields->m_english.clear();
        m_fields->m_korean.clear();

        // 이미 한글인 글자는 다른 한국어 패치가 먼저 바꿔 놓은 것이다. 거기에
        // 또 손을 대면 두 패치가 같은 라벨을 두고 서로 밀어내게 된다.
        //
        // 다만 글꼴에 한글이 없으면 빈칸으로 그려진다. Geode 가 이 모드의 한국어
        // 설명을 그릴 때가 그랬다. 우리 글꼴을 쓰기로 했으면 글은 그대로 두고
        // 글꼴만 바꾼다. 다른 한국어 글꼴을 쓰는 사람은 그 설정을 끄면 된다.
        if (kopatch::containsHangul(text)) {
            if (translator.ownFont()) {
                std::string const same(text);
                this->applyKorean(same, same, needUpdateLabel);
                return;
            }
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
