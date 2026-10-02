#pragma once

namespace cocos2d { class CCNode; }

namespace kopatch::loading {
    // 로딩 화면은 우리 모드가 켜지기 전에 다 지어진다. 그래서 LoadingLayer::init
    // 훅은 첫 로딩 화면에서 한 번도 돌지 못한다. main.cpp 가 우리가 켜지는
    // 순간 이 둘을 불러 이미 떠 있는 화면을 손본다.

    // 여러 줄 글 상자(팁)를 통째로 다시 짓는다. 줄마다 따로 번역하면 두 줄로
    // 나뉜 팁은 토막이 되어 표에 걸리지 않는다. 라벨을 다시 올리기 전에 부른다.
    void rebuildText();

    // 위와 같지만 아무 마디 아래나 훑는다. 끝 화면처럼 번역을 잠시 멈춘 채
    // 지어진 화면도 같은 꼴이라 함께 쓴다.
    void rebuildTextIn(cocos2d::CCNode* root);

    // 가운데 언저리에 있던 글을 화면 한가운데에 다시 앉힌다. 라벨을 다시
    // 올린 뒤에 부른다.
    void centreText();
}
