#pragma once

#include <Geode/Geode.hpp>

#include <string>
#include <vector>

namespace gdu::ui {
    // 판 위의 작은 표. 왼쪽 위에 난이도와 점수, 고른 카드 세 칸을 보여 준다.
    //
    //  ┌──────────────────────┐
    //  │ (얼굴) EASY      145 │
    //  │ [카드][카드][25%]  x1.2 │
    //  └──────────────────────┘
    class RunHud : public cocos2d::CCNode {
    public:
        static RunHud* create(std::string const& tier, std::vector<int> const& milestones);

        // 고른 카드와 점수를 다시 그린다. animate 면 마지막 칸이 튀어 오르고 점수가 차오른다.
        // usedShields 장의 방패는 이미 깨진 것으로 회색이 된다.
        void refresh(std::vector<std::string> const& picks, int score, int usedShields, float speed, bool animate);
        // 죽어서 카드를 모두 잃었을 때. 칸이 비고 판이 붉게 번쩍인다.
        void lost();
        // 방패가 하나 깨졌을 때.
        void shieldBroke();
        // 깼을 때 화면 위쪽에 띄우는 점수.
        void showClear(int score, std::vector<std::string> const& picks);

        // 카드가 날아올 칸의 화면 좌표.
        cocos2d::CCPoint slotWorld(std::size_t index);

    private:
        bool init(std::string const& tier, std::vector<int> const& milestones);
        void update(float dt) override;

        struct Slot {
            cocos2d::CCSprite* frame = nullptr;
            cocos2d::CCNode* icon = nullptr;
            cocos2d::CCLabelBMFont* empty = nullptr;
            std::string card;
        };

        cocos2d::CCNode* m_panel = nullptr;
        cocos2d::CCLabelBMFont* m_score = nullptr;
        cocos2d::CCLabelBMFont* m_speed = nullptr;
        std::vector<Slot> m_slots;
        float m_shown = 0.f;
        int m_target = 0;
    };
}
