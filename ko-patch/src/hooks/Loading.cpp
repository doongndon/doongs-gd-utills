#include <Geode/Geode.hpp>
#include <Geode/modify/LoadingLayer.hpp>

using namespace geode::prelude;

// 로딩 화면의 팁은 영어 한 줄에 맞춰 자리가 잡혀 있다. 그 자리는 글의 너비를
// 재서 정한 것이라, 한국어가 영어보다 좁으면 왼쪽으로 밀리고 넓으면 오른쪽으로
// 밀린다. 글자를 바꾼 것은 우리이니 자리도 우리가 맞춰 놓는다.
//
// 이 화면에 글은 팁과 Geode 의 작은 줄 몇 개뿐이고, 그것들은 원래 가운데에
// 있으므로 가운데로 옮겨도 달라지지 않는다.
class $modify(KoreanLoadingLayer, LoadingLayer) {
    bool init(bool fromReload) {
        if (!LoadingLayer::init(fromReload)) {
            return false;
        }

        float const middle = CCDirector::sharedDirector()->getWinSize().width / 2.f;
        for (auto* child : CCArrayExt<CCNode*>(this->getChildren())) {
            if (!typeinfo_cast<CCLabelBMFont*>(child) && !typeinfo_cast<TextArea*>(child)) {
                continue;
            }
            child->setAnchorPoint({ 0.5f, child->getAnchorPoint().y });
            child->setPositionX(middle);
        }
        return true;
    }
};
