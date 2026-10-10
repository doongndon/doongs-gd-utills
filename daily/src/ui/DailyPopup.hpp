#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/web.hpp>

#include <optional>
#include <string>
#include <vector>

namespace gdu::ui {
    // 오늘의 도전 창.
    //
    //  ┌──────────────────── Daily Challenge ─────────────────── (i) ┐
    //  │   (Main)(Easy)(Normal)(Hard)(Harder)(Insane)(Demon)          │  난이도 탭
    //  │ ┌─ 레벨 ──────────────┐ ┌─[Ranking][Cards]──────────── (↻) ┐ │
    //  │ │ (얼굴)       (▶)    │ │ 1  doong     [][][]       145 │ │
    //  │ │ ★4                  │ │ 2  player2   [][][]       130 │ │
    //  │ │ Level name          │ │ ...                            │ │
    //  │ │ by Creator          │ │                                │ │
    //  │ │ BEST 145 PTS · #3   │ │                                │ │
    //  │ └─────────────────────┘ └────────────────────────────────┘ │
    //  │                 New challenge in 05:12:33                    │
    //  └──────────────────────────────────────────────────────────────┘
    class DailyPopup : public geode::Popup {
    public:
        static DailyPopup* create();

    protected:
        bool init();

    private:
        enum class View { Ranking, Cards };

        struct Tab {
            std::string id;
            CCMenuItemSpriteExtra* button = nullptr;
            cocos2d::CCNode* holder = nullptr;
            cocos2d::CCNode* face = nullptr;
            cocos2d::CCLabelBMFont* label = nullptr;
            cocos2d::CCSprite* glow = nullptr;
            cocos2d::CCSprite* check = nullptr;
        };

        void buildTabs();
        void paintTabs();
        void buildLevelPanel();
        void buildBoardPanel();

        void refresh();
        void render(std::optional<std::string> const& error);
        void renderLevel(std::optional<std::string> const& error);
        void renderRanking();
        void renderCards();
        void renderTimer();
        void setLoading(bool loading);
        void clearChips();

        void tick(float);
        void onTab(cocos2d::CCObject* sender);
        void onView(cocos2d::CCObject* sender);
        void onPlay(cocos2d::CCObject*);
        void onRefresh(cocos2d::CCObject*);
        void onInfo(cocos2d::CCObject*);

        std::string m_tier;
        View m_view = View::Ranking;
        bool m_loading = false;
        // 오늘의 도전을 한 번이라도 받았는지. 자정이 지나 그것이 끝나면 한 번만 새로 받는다.
        bool m_hadToday = false;
        // 마지막으로 받은 답의 오류와, 마지막으로 그린 때의 daily::revision().
        std::optional<std::string> m_error;
        int m_seenRevision = -1;

        std::vector<Tab> m_tabs;

        cocos2d::CCNode* m_levelPanel = nullptr;
        cocos2d::CCNode* m_levelArt = nullptr;
        cocos2d::CCLabelBMFont* m_levelName = nullptr;
        cocos2d::CCLabelBMFont* m_creator = nullptr;
        cocos2d::CCLabelBMFont* m_info = nullptr;
        cocos2d::CCLabelBMFont* m_status = nullptr;
        cocos2d::CCLabelBMFont* m_detail = nullptr;
        CCMenuItemSpriteExtra* m_playButton = nullptr;

        cocos2d::CCNode* m_boardPanel = nullptr;
        cocos2d::CCNode* m_rows = nullptr;
        cocos2d::CCLabelBMFont* m_empty = nullptr;
        cocos2d::CCNode* m_spinner = nullptr;
        ButtonSprite* m_rankingTab = nullptr;
        ButtonSprite* m_cardsTab = nullptr;
        std::vector<CCMenuItemSpriteExtra*> m_chips;

        cocos2d::CCLabelBMFont* m_timer = nullptr;

        // 팝업이 닫히면 이것도 사라지며 기다리던 요청을 거둔다. 닫힌 팝업에 답을
        // 그리려다 터지는 일이 없다.
        geode::async::TaskHolder<geode::utils::web::WebResponse> m_task;
    };
}
