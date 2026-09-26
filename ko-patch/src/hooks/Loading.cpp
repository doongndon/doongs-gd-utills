#include <Geode/Geode.hpp>
#include <Geode/modify/LoadingLayer.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

using namespace geode::prelude;

namespace {
    // 이 화면의 글은 영어 한 줄에 맞춰 자리가 잡혀 있다. 그 자리는 글의 너비를
    // 재서 정한 것이라, 한국어가 영어보다 좁으면 왼쪽으로 밀리고 넓으면
    // 오른쪽으로 밀린다. 글자를 바꾼 것은 우리이니 자리도 우리가 맞춘다.
    //
    // 다만 원래부터 구석에 있던 글까지 끌어다 가운데에 놓으면 안 된다.
    // 그래서 "이미 가운데 언저리에 있는" 글만 손댄다.
    //
    // NEAR 라는 이름은 쓰지 않는다. 윈도우 헤더가 이미 그 이름을 매크로로
    // 쓰고 있어서, 그대로 두면 윈도우에서만 빌드가 깨진다.
    constexpr float MIDDLE_SLACK = 0.15f;   // 화면 너비의 이만큼까지가 가운데 언저리다

    // 글자가 화면에서 실제로 차지한 폭을 잰다.
    //
    // 마디의 contentSize 를 믿으면 안 된다. TextArea 의 그것은 글이 차지한
    // 폭이 아니라 줄바꿈 한계 폭이라, 한국어가 그보다 좁으면 남는 폭의 절반만큼
    // 어긋난다. 옷 길이를 재야 할 자리에서 옷걸이 길이를 재는 셈이다.
    //
    // 그래서 그려지는 글자 라벨을 끝까지 따라 내려가 그 양끝을 화면 좌표로
    // 재어 온다. 눈에 보이는 것을 그대로 재는 셈이다.
    bool measure(CCNode* node, float& left, float& right) {
        if (!node || !node->isVisible()) return false;

        bool found = false;
        if (auto* label = typeinfo_cast<CCLabelBMFont*>(node)) {
            auto const width = label->getContentSize().width;
            if (width > 0.f) {
                auto const a = label->convertToWorldSpace(ccp(0.f, 0.f));
                auto const b = label->convertToWorldSpace(ccp(width, 0.f));
                left = std::min({ left, a.x, b.x });
                right = std::max({ right, a.x, b.x });
                found = true;
            }
        }

        if (auto* children = node->getChildren()) {
            for (auto* child : CCArrayExt<CCNode*>(children)) {
                found |= measure(child, left, right);
            }
        }
        return found;
    }

    void centreLabels(CCNode* node, float middle, float slack) {
        if (!node) return;
        auto* children = node->getChildren();
        if (!children) return;

        for (auto* child : CCArrayExt<CCNode*>(children)) {
            if (!child) continue;

            bool const isText = typeinfo_cast<CCLabelBMFont*>(child)
                             || typeinfo_cast<TextArea*>(child);
            if (!isText) {
                centreLabels(child, middle, slack);
                continue;
            }

            auto* parent = child->getParent();
            if (!parent) continue;

            float left = std::numeric_limits<float>::max();
            float right = std::numeric_limits<float>::lowest();
            if (!measure(child, left, right)) continue;

            float const centre = (left + right) / 2.f;
            if (std::fabs(centre - middle) > slack) continue;

            // 화면에서 옮겨야 할 만큼을 이 마디의 부모 자리로 바꿔 민다.
            auto const from = parent->convertToNodeSpace(ccp(centre, 0.f));
            auto const to = parent->convertToNodeSpace(ccp(middle, 0.f));
            child->setPositionX(child->getPositionX() + (to.x - from.x));
        }
    }
}

class $modify(KoreanLoadingLayer, LoadingLayer) {
    void centreLoadingText(float) {
        float const width = CCDirector::sharedDirector()->getWinSize().width;
        centreLabels(this, width / 2.f, width * MIDDLE_SLACK);
    }

    bool init(bool fromReload) {
        if (!LoadingLayer::init(fromReload)) {
            return false;
        }

        float const width = CCDirector::sharedDirector()->getWinSize().width;
        centreLabels(this, width / 2.f, width * MIDDLE_SLACK);
        // Geode가 모드를 읽는 동안 하단 상태 문구와 팁을 계속 갱신한다.
        // init 직후 한 번만 재면 첫 문장의 폭에 고정되어, 번역된 문구나
        // 모드 수가 바뀐 뒤에는 가운데에서 벗어난다.
        this->schedule(schedule_selector(KoreanLoadingLayer::centreLoadingText), 0.1f);
        return true;
    }
};
