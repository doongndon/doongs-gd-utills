#include <Geode/Geode.hpp>
#include <Geode/modify/EndLevelLayer.hpp>

#include <string>

#include "KoreanFont.hpp"

using namespace geode::prelude;

namespace {
    // NodeIDs 는 끝 화면의 노드에 이름을 세어서 붙인다. 자식을 훑다가 "Attempts",
    // "Jumps", "Time", "Points" 로 시작하는 라벨을 만날 때마다 수를 하나씩
    // 올리고, 그 수를 그대로 자리 번호로 삼아 단추 메뉴를 찾아 "button-menu"
    // 라고 이름 붙인다. 우리가 그 글자를 "시도" 로 바꿔 두면 하나도 걸리지
    // 않아 수가 덜 오르고, button-menu 라는 이름이 메뉴가 아니라 라벨에 찍힌다.
    // 그 다음 Eclipse 가 그 이름으로 찾아온 라벨에 제 표시를 붙이려다 게임이
    // 터진다.
    //
    // 그래서 이 화면이 만들어지는 동안에는 번역을 멈춘다. GD 도 NodeIDs 도
    // 영어를 보고 제 일을 끝낸 뒤, 그제서야 우리가 한 번에 바꾼다.
    void retranslate(CCNode* node) {
        if (!node) return;
        if (auto* label = typeinfo_cast<CCLabelBMFont*>(node)) {
            if (char const* text = label->getString(); text && *text) {
                std::string const copy(text);
                label->setString(copy.c_str());
            }
        }
        auto* children = node->getChildren();
        if (!children) return;
        for (auto* child : CCArrayExt<CCNode*>(children)) {
            retranslate(child);
        }
    }
}

class $modify(KoreanEndLevelLayer, EndLevelLayer) {
    void customSetup() {
        kopatch::setSplittingText(true);
        EndLevelLayer::customSetup();
        kopatch::setSplittingText(false);

        retranslate(this);
    }
};
