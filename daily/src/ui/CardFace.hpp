#pragma once

#include <Geode/Geode.hpp>

#include <string>

namespace gdu::ui {
    // 카드 한 장. make_art.py 가 그린 틀 위에 GD 글꼴로 종류, 이름, 설명, 점수를 얹는다.
    // 크기는 WIDTH x HEIGHT, 기준점은 가운데다. 글자 자리는 make_art.py 의 CARD_* 와 같다.
    class CardFace : public cocos2d::CCNode {
    public:
        static constexpr float WIDTH = 120.f;
        static constexpr float HEIGHT = 168.f;

        static CardFace* create(std::string const& id);

        // 앞면과 뒷면을 바꾼다. 뒤집는 움직임은 부르는 쪽이 scaleX 로 만든다.
        void showFront(bool front);
        bool isHelp() const;
        std::string const& cardID() const;

    private:
        bool init(std::string const& id);

        cocos2d::CCNode* m_front = nullptr;
        cocos2d::CCNode* m_back = nullptr;
        std::string m_id;
        bool m_help = false;
    };

    // 작은 카드 조각. 팝업의 오늘의 카드 목록에 쓴다. 기준점은 가운데.
    cocos2d::CCNode* cardChip(std::string const& id, float width, float height);
}
