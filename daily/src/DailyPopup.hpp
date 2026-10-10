#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>

#include <optional>
#include <string>
#include <vector>

namespace gdu {
    // 오늘의 도전 창.
    //
    //  ┌──────────────── Daily Challenge ────────────────┐
    //  │ [Main][Easy][Normal][Hard][Harder][Insane][...]  │  난이도 탭
    //  │ ┌─ 레벨 ───────────┐ ┌─ 순위표 ───────────────┐ │
    //  │ │  (얼굴)          │ │ 1  doong          37   │ │
    //  │ │  Level name      │ │ 2  player2        52   │ │
    //  │ │  내 기록          │ │ ...                    │ │
    //  │ │  [ Play ]        │ │                        │ │
    //  │ └──────────────────┘ └────────────────────────┘ │
    //  │ 남은 시간                              (새로고침) │
    //  └──────────────────────────────────────────────────┘
    class DailyPopup : public geode::Popup {
    public:
        static DailyPopup* create();

    private:
        bool init();

        void buildTabs();
        void paintTabs();
        void buildLevelPanel();
        void buildBoardPanel();

        void refresh();
        void render(std::optional<std::string> const& error);
        void renderLevel(std::optional<std::string> const& error);
        void renderBoard();
        void setLoading(bool loading);

        void onTab(cocos2d::CCObject* sender);
        void onPlay(cocos2d::CCObject*);
        void onRefresh(cocos2d::CCObject*);

        std::string m_tier;
        bool m_loading = false;

        std::vector<ButtonSprite*> m_tabSprites;

        cocos2d::CCNode* m_levelPanel = nullptr;
        cocos2d::CCSprite* m_face = nullptr;
        cocos2d::CCLabelBMFont* m_tierName = nullptr;
        cocos2d::CCLabelBMFont* m_levelName = nullptr;
        cocos2d::CCLabelBMFont* m_status = nullptr;
        cocos2d::CCLabelBMFont* m_rules = nullptr;
        CCMenuItemSpriteExtra* m_playButton = nullptr;

        cocos2d::CCNode* m_boardPanel = nullptr;
        cocos2d::CCNode* m_rows = nullptr;
        cocos2d::CCLabelBMFont* m_boardTitle = nullptr;
        cocos2d::CCLabelBMFont* m_boardEmpty = nullptr;
        cocos2d::CCNode* m_spinner = nullptr;

        cocos2d::CCLabelBMFont* m_footer = nullptr;

        // 팝업이 닫히면 이것도 사라지며 기다리던 요청을 거둔다. 닫힌 팝업에 답을
        // 그리려다 터지는 일이 없다.
        geode::async::TaskHolder<geode::utils::web::WebResponse> m_task;
    };
}
