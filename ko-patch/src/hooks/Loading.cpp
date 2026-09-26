#include <Geode/Geode.hpp>
#include <Geode/modify/LoadingLayer.hpp>

#include <cmath>

using namespace geode::prelude;

namespace {
    // 이 화면의 글은 영어 한 줄에 맞춰 자리가 잡혀 있다. 그 자리는 글의 너비를
    // 재서 정한 것이라, 한국어가 영어보다 좁으면 왼쪽으로 밀리고 넓으면
    // 오른쪽으로 밀린다. 글자를 바꾼 것은 우리이니 자리도 우리가 맞춘다.
    //
    // 다만 원래부터 구석에 있던 글까지 끌어다 가운데에 놓으면 안 된다.
    // 그래서 "이미 가운데 언저리에 있는" 글만 손댄다. 제자리에서 조금
    // 밀린 것은 되돌리고, 애초에 한쪽에 있던 것은 그대로 둔다.
    // NEAR 라는 이름은 쓰지 않는다. 윈도우 헤더가 이미 그 이름을 매크로로
    // 쓰고 있어서, 그대로 두면 윈도우에서만 빌드가 깨진다.
    constexpr float MIDDLE_SLACK = 0.15f;   // 화면 너비의 이만큼까지가 가운데 언저리다

    void centreLabels(CCNode* node, float middle, float slack) {
        if (!node) return;
        auto* children = node->getChildren();
        if (!children) return;

        for (auto* child : CCArrayExt<CCNode*>(children)) {
            if (!child) continue;

            bool const isText = typeinfo_cast<CCLabelBMFont*>(child)
                             || typeinfo_cast<TextArea*>(child);
            if (isText) {
                // 지금 이 글의 한가운데가 화면 어디에 있는지 본다. 자리는
                // 부모 기준이므로, 부모가 원점에 있는 흔한 경우에만 맞는다.
                float const width = child->getContentSize().width * child->getScaleX();
                float const centre =
                    child->getPositionX() + (0.5f - child->getAnchorPoint().x) * width;

                if (std::fabs(centre - middle) <= slack) {
                    child->setPositionX(child->getPositionX() + (middle - centre));
                }
                continue;   // 글 안쪽은 더 들어가지 않는다
            }

            centreLabels(child, middle, slack);
        }
    }
}

class $modify(KoreanLoadingLayer, LoadingLayer) {
    bool init(bool fromReload) {
        if (!LoadingLayer::init(fromReload)) {
            return false;
        }

        float const width = CCDirector::sharedDirector()->getWinSize().width;
        centreLabels(this, width / 2.f, width * MIDDLE_SLACK);
        return true;
    }
};
