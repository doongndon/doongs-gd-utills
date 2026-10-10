#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/utils/function.hpp>

#include <string>
#include <vector>

namespace gdu::ui {
    class CardFace;

    // 카드를 고르는 화면. 판 위에 Popup 으로 띄운다. Popup 은 GD 의 다른 화면보다
    // 터치를 먼저 받으므로, 점프하려고 화면을 두드리던 손가락이 아래의 게임으로 새지 않는다.
    //
    // 흐름: 카드 세 장이 뒷면으로 깔리고 → 한 장씩 뒤집히고 → 고르면 →
    // 고른 카드가 HUD 의 칸으로 날아가고 → 3, 2, 1, GO 를 센 뒤 사라진다.
    // 깔리고 뒤집히는 동안에는 고를 수 없다. 점프하던 손가락에 카드가 잘못 골리지 않게.
    class CardPicker : public geode::Popup {
    public:
        struct Hooks {
            // 카드를 고른 순간. 효과를 건다.
            geode::Function<void(std::string const&)> picked;
            // 고른 카드가 날아갈 HUD 칸의 화면 좌표.
            geode::Function<cocos2d::CCPoint()> target;
            // 카드가 HUD 칸에 닿은 순간.
            geode::Function<void()> landed;
            // GO. 게임을 다시 돌린다.
            geode::Function<void()> resume;
        };

        static CardPicker* create(int percent, std::vector<std::string> const& offer, Hooks hooks);

        void show() override;
        // 판이 끝나(다시 시작, 나가기) 치울 때. 아무 콜백도 부르지 않는다.
        void dismiss();

    protected:
        bool init(int percent, std::vector<std::string> const& offer, Hooks hooks);
        void keyBackClicked() override;
        void keyDown(cocos2d::enumKeyCodes key, double timestamp) override;
        void onClose(cocos2d::CCObject*) override;

    private:
        void flip(std::size_t index);
        void ready();
        void focus(int index);
        void onCard(cocos2d::CCObject* sender);
        void pick(std::size_t index);
        void fly();
        void count(int value);
        void finish();

        Hooks m_hooks;
        int m_percent = 0;
        std::vector<std::string> m_offer;
        std::vector<CCMenuItemSpriteExtra*> m_buttons;
        std::vector<CardFace*> m_faces;
        std::vector<cocos2d::CCSprite*> m_glows;
        cocos2d::CCLabelBMFont* m_title = nullptr;
        cocos2d::CCLabelBMFont* m_subtitle = nullptr;
        cocos2d::CCLabelBMFont* m_hint = nullptr;
        cocos2d::CCLabelBMFont* m_rule = nullptr;
        cocos2d::CCLabelBMFont* m_count = nullptr;
        CardFace* m_flying = nullptr;
        cocos2d::CCSprite* m_burst = nullptr;
        bool m_ready = false;
        bool m_picked = false;
        bool m_finished = false;
        int m_focus = -1;
    };
}
