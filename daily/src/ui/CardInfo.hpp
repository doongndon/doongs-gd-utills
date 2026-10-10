#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>

#include <string>

namespace gdu::ui {
    // 카드 한 장을 크게 보여 준다. 오늘의 카드 목록에서 카드를 누르면 뜬다.
    // 아무 데나 누르거나 뒤로 가기를 누르면 닫힌다.
    class CardInfo : public geode::Popup {
    public:
        static CardInfo* create(std::string const& id);
        void show() override;

    protected:
        bool init(std::string const& id);

    private:
        cocos2d::CCNode* m_card = nullptr;
    };
}
