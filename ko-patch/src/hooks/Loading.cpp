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
    //
    // NEAR 라는 이름은 쓰지 않는다. 윈도우 헤더가 이미 그 이름을 매크로로
    // 쓰고 있어서, 그대로 두면 윈도우에서만 빌드가 깨진다.
    constexpr float MIDDLE_SLACK = 0.15f;   // 화면 너비의 이만큼까지가 가운데 언저리다

    // 마디의 자리는 부모 기준으로 적힌다. 한 겹 안에 든 글을 화면 기준으로
    // 옮기려면 두 자리 사이를 옮겨 주어야 한다. 부모가 원점에 없거나 배율이
    // 걸려 있으면 그냥 화면 좌표를 적어 넣는 것으로는 어긋난다.
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

            // 글은 가운데를 잡고 놓는다. 영어 너비에 맞춰 왼쪽 끝을 박아 둔
            // 라벨이라도, 가운데를 잡아 두면 글자가 길어지든 짧아지든
            // 한가운데가 움직이지 않는다.
            float const y = child->getAnchorPoint().y;
            float const before = child->getContentSize().width
                               * child->getScaleX()
                               * (0.5f - child->getAnchorPoint().x);
            float const centre = child->getPositionX() + before;

            // 지금 이 글의 한가운데가 화면에서 어디인지 본다.
            auto const world = parent->convertToWorldSpace(ccp(centre, 0.f));
            if (std::fabs(world.x - middle) > slack) continue;

            // 화면 한가운데를 이 마디의 부모 자리로 바꿔 적는다.
            auto const target = parent->convertToNodeSpace(ccp(middle, world.y));
            child->setAnchorPoint({ 0.5f, y });
            child->setPositionX(target.x);
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
