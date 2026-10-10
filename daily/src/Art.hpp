#pragma once

#include <Geode/Geode.hpp>

#include <string>
#include <string_view>

// 모드가 쓰는 그림과 소리. 그림은 daily/tools/make_art.py, 소리는 make_sounds.py 가
// 만들어 daily/resources/ 에 둔 것이다. 묶음에서 빠졌거나 못 읽으면 GD 의 그림으로
// 물러선다. 그림 하나가 없다고 화면이 통째로 비거나 게임이 멈추지 않게.
namespace gdu::art {
    // 카드 종류의 바탕색. 도움은 민트, 저주는 분홍. 그림의 색과 맞춘다.
    cocos2d::ccColor3B kindColor(bool help);
    // 같은 색의 밝은 쪽. 글자에 쓴다.
    cocos2d::ccColor3B kindLight(bool help);
    // 점수 글자 색. 깎이면 붉게, 오르면 금색.
    cocos2d::ccColor3B pointsColor(int points);
    // "+25", "-30"
    std::string pointsText(int points);

    // 모드에 묶인 그림 하나. 없으면 nullptr.
    cocos2d::CCSprite* sprite(std::string_view name);
    // 카드 그림. 모르는 카드면 GD 의 별을 준다.
    cocos2d::CCSprite* cardIcon(std::string const& id);

    // GD 의 난이도 얼굴(글자 없는 것). number 는 GD 그림 번호(diffIcon_XX_btn_001.png)다.
    // 1~5 이지~인세인, 6 하드 데몬, 7 이지 데몬, 8 미디엄, 9 인세인 데몬, 10 익스트림, 0 N/A.
    // feature 가 있으면(1 피처드, 2 에픽, 3 레전더리, 4 미식) 뒤에 테를 두른다.
    cocos2d::CCSprite* difficultyFace(int number, int feature = 0);

    // 가장 긴 변이 size 가 되게 줄이거나 늘린다.
    void fit(cocos2d::CCNode* node, float size);

    // 뒤의 색에 빛을 더하듯 그린다(가산 혼합). 빛나는 그림에 쓴다.
    void additive(cocos2d::CCSprite* sprite);

    // 효과음. 모드 설정에서 끄면 내지 않는다.
    void play(std::string_view sound, float volume = 0.6f);
}
