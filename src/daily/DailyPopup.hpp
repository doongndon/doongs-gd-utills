#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>

#include <optional>
#include <string>
#include <vector>

namespace gdu {
    class DailyPopup : public geode::Popup {
    public:
        static DailyPopup* create();

    private:
        bool init();

        void buildTierRow();
        void paintTierRow();
        void refresh();
        void render(std::optional<std::string> const& error);

        void onTier(cocos2d::CCObject* sender);
        void onPlay(cocos2d::CCObject*);
        void onRefresh(cocos2d::CCObject*);

        std::string m_tier;
        cocos2d::CCMenu* m_tierMenu = nullptr;
        std::vector<ButtonSprite*> m_tierSprites;
        cocos2d::CCLabelBMFont* m_level = nullptr;
        cocos2d::CCLabelBMFont* m_status = nullptr;
        cocos2d::CCLabelBMFont* m_board = nullptr;
        CCMenuItemSpriteExtra* m_playButton = nullptr;

        // 팝업이 닫히면 이것도 사라지며 기다리던 요청을 거둔다. 닫힌 팝업에 답을
        // 그리려다 터지는 일이 없다.
        geode::async::TaskHolder<geode::utils::web::WebResponse> m_task;
    };
}
