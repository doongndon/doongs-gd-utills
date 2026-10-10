#include "DailyPopup.hpp"

#include <Geode/ui/LoadingSpinner.hpp>
#include <Geode/ui/NineSlice.hpp>

#include <algorithm>
#include <array>
#include <chrono>

#include <fmt/format.h>

#include "Daily.hpp"

using namespace geode::prelude;

namespace {
    constexpr float WIDTH = 420.f;
    constexpr float HEIGHT = 290.f;

    constexpr float PANEL_BOTTOM = 40.f;
    constexpr float PANEL_HEIGHT = 178.f;
    constexpr float LEVEL_X = 15.f;
    constexpr float LEVEL_W = 160.f;
    constexpr float BOARD_X = 183.f;
    constexpr float BOARD_W = 222.f;
    constexpr float TABS_Y = HEIGHT - 52.f;

    constexpr std::size_t ROWS = 10;
    constexpr float ROW_TOP = PANEL_HEIGHT - 42.f;
    constexpr float ROW_STEP = 12.6f;

    struct Tab {
        char const* id;
        char const* label;
    };

    // 서버의 난이도 목록(daily-server/server.js 의 TIERS)과 같은 차례다. 서버의 답이
    // 오기 전에도 탭을 그릴 수 있게 여기에도 적어 둔다.
    constexpr std::array<Tab, 8> TABS = {{
        { "main", "Main" },
        { "easy", "Easy" },
        { "normal", "Normal" },
        { "hard", "Hard" },
        { "harder", "Harder" },
        { "insane", "Insane" },
        { "easy-demon", "Demon" },
        { "roguelike", "Rogue" },
    }};

    // 난이도 칸마다 보여 줄 그림. 얼굴은 GD 의 난이도 얼굴 그림이다.
    char const* faceOf(std::string_view tier) {
        if (tier == "easy") return "difficulty_01_btn_001.png";
        if (tier == "normal") return "difficulty_02_btn_001.png";
        if (tier == "hard") return "difficulty_03_btn_001.png";
        if (tier == "harder") return "difficulty_04_btn_001.png";
        if (tier == "insane") return "difficulty_05_btn_001.png";
        if (tier == "easy-demon") return "GJ_demonIcon_001.png";
        if (tier == "roguelike") return "GJ_sMagicIcon_001.png";
        return "GJ_completesIcon_001.png";
    }

    // 어두운 반투명 판. Geode 의 모드 목록이 쓰는 것과 같은 그림이다.
    CCNode* panel(float width, float height) {
        auto* bg = NineSlice::create("square02b_001.png");
        bg->setContentSize({ width, height });
        bg->setColor({ 0, 0, 0 });
        bg->setOpacity(80);
        bg->setAnchorPoint({ 0.f, 0.f });
        bg->setPosition({ 0.f, 0.f });
        return bg;
    }

    CCLabelBMFont* label(CCNode* parent, char const* font, float scale, CCPoint position, CCPoint anchor = { 0.5f, 0.5f }) {
        auto* text = CCLabelBMFont::create("", font);
        text->setScale(scale);
        text->setAnchorPoint(anchor);
        text->setPosition(position);
        parent->addChild(text);
        return text;
    }

    std::string timeLeft(int seconds) {
        if (seconds <= 0) {
            return "A new challenge is starting";
        }
        int const hours = seconds / 3600;
        int const minutes = seconds % 3600 / 60;
        return hours > 0 ? fmt::format("New challenge in {}h {:02}m", hours, minutes)
                         : fmt::format("New challenge in {}m", std::max(1, minutes));
    }
}

namespace gdu {
    DailyPopup* DailyPopup::create() {
        auto* popup = new DailyPopup();
        if (popup->init()) {
            popup->autorelease();
            return popup;
        }
        delete popup;
        return nullptr;
    }

    bool DailyPopup::init() {
        if (!Popup::init(WIDTH, HEIGHT)) {
            return false;
        }
        this->setTitle("Daily Challenge");
        m_tier = daily::selectedTier();

        this->buildTabs();
        this->buildLevelPanel();
        this->buildBoardPanel();

        m_footer = label(m_mainLayer, "chatFont.fnt", 0.55f, { LEVEL_X + 2.f, 22.f }, { 0.f, 0.5f });
        m_footer->setOpacity(170);

        auto* refresh = ButtonSprite::create("Refresh", "goldFont.fnt", "GJ_button_04.png", 0.8f);
        auto* refreshButton = CCMenuItemSpriteExtra::create(refresh, this, menu_selector(DailyPopup::onRefresh));
        refreshButton->setScale(0.55f);
        refreshButton->m_baseScale = 0.55f;
        refreshButton->setPosition({ WIDTH - 15.f - refreshButton->getScaledContentSize().width / 2.f, 22.f });
        m_buttonMenu->addChild(refreshButton);

        // 받아 둔 것이 있으면 먼저 보여 주고, 뒤에서 새로 받아 바꾼다.
        this->render(std::nullopt);
        this->refresh();
        return true;
    }

    // 난이도 탭. 처음 한 번만 짓고, 고를 때는 배경만 바꿔 칠한다. 누른 단추를 그 단추의
    // 콜백 안에서 지웠다 다시 지으면 아직 돌고 있는 단추를 지우는 셈이 된다.
    void DailyPopup::buildTabs() {
        float const slot = (WIDTH - 30.f) / static_cast<float>(TABS.size());
        for (std::size_t i = 0; i < TABS.size(); ++i) {
            auto* sprite = ButtonSprite::create(TABS[i].label, "bigFont.fnt", "GJ_button_04.png", 0.6f);
            auto* button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(DailyPopup::onTab));
            button->setTag(static_cast<int>(i));

            float const width = button->getContentSize().width;
            float const scale = width > 0.f ? std::min(0.62f, (slot - 3.f) / width) : 0.6f;
            button->setScale(scale);
            button->m_baseScale = scale;
            button->setPosition({ 15.f + slot * (static_cast<float>(i) + 0.5f), TABS_Y });
            m_buttonMenu->addChild(button);
            m_tabSprites.push_back(sprite);
        }
        this->paintTabs();
    }

    void DailyPopup::paintTabs() {
        for (std::size_t i = 0; i < m_tabSprites.size() && i < TABS.size(); ++i) {
            m_tabSprites[i]->updateBGImage(m_tier == TABS[i].id ? "GJ_button_01.png" : "GJ_button_04.png");
        }
    }

    void DailyPopup::buildLevelPanel() {
        m_levelPanel = CCNode::create();
        m_levelPanel->setContentSize({ LEVEL_W, PANEL_HEIGHT });
        m_levelPanel->setPosition({ LEVEL_X, PANEL_BOTTOM });
        m_mainLayer->addChild(m_levelPanel);
        m_levelPanel->addChild(panel(LEVEL_W, PANEL_HEIGHT));

        float const middle = LEVEL_W / 2.f;
        m_tierName = label(m_levelPanel, "goldFont.fnt", 0.6f, { middle, PANEL_HEIGHT - 16.f });
        m_levelName = label(m_levelPanel, "bigFont.fnt", 0.5f, { middle, PANEL_HEIGHT - 92.f });

        m_status = CCLabelBMFont::create("", "chatFont.fnt", (LEVEL_W - 16.f) / 0.55f, kCCTextAlignmentCenter);
        m_status->setScale(0.55f);
        m_status->setPosition({ middle, PANEL_HEIGHT - 114.f });
        m_levelPanel->addChild(m_status);

        m_rules = CCLabelBMFont::create("", "chatFont.fnt", (LEVEL_W - 16.f) / 0.45f, kCCTextAlignmentCenter);
        m_rules->setScale(0.45f);
        m_rules->setOpacity(160);
        m_rules->setPosition({ middle, PANEL_HEIGHT - 136.f });
        m_levelPanel->addChild(m_rules);

        auto* play = ButtonSprite::create("Play", "goldFont.fnt", "GJ_button_01.png", 0.9f);
        m_playButton = CCMenuItemSpriteExtra::create(play, this, menu_selector(DailyPopup::onPlay));
        m_playButton->setScale(0.75f);
        m_playButton->m_baseScale = 0.75f;
        m_playButton->setPosition({ LEVEL_X + middle, PANEL_BOTTOM + 18.f });
        m_buttonMenu->addChild(m_playButton);
    }

    void DailyPopup::buildBoardPanel() {
        m_boardPanel = CCNode::create();
        m_boardPanel->setContentSize({ BOARD_W, PANEL_HEIGHT });
        m_boardPanel->setPosition({ BOARD_X, PANEL_BOTTOM });
        m_mainLayer->addChild(m_boardPanel);
        m_boardPanel->addChild(panel(BOARD_W, PANEL_HEIGHT));

        m_boardTitle = label(m_boardPanel, "goldFont.fnt", 0.6f, { BOARD_W / 2.f, PANEL_HEIGHT - 16.f });
        m_boardTitle->setString("Leaderboard");

        m_rows = CCNode::create();
        m_rows->setContentSize({ BOARD_W, PANEL_HEIGHT });
        m_boardPanel->addChild(m_rows);

        m_boardEmpty = CCLabelBMFont::create("", "chatFont.fnt", (BOARD_W - 30.f) / 0.6f, kCCTextAlignmentCenter);
        m_boardEmpty->setScale(0.6f);
        m_boardEmpty->setOpacity(170);
        m_boardEmpty->setPosition({ BOARD_W / 2.f, PANEL_HEIGHT / 2.f - 6.f });
        m_boardPanel->addChild(m_boardEmpty);

        auto* spinner = LoadingSpinner::create(28.f);
        spinner->setPosition({ BOARD_W / 2.f, PANEL_HEIGHT / 2.f - 6.f });
        spinner->setVisible(false);
        m_boardPanel->addChild(spinner);
        m_spinner = spinner;
    }

    void DailyPopup::setLoading(bool loading) {
        m_loading = loading;
        m_spinner->setVisible(loading);
        if (loading) {
            m_boardEmpty->setString("");
        }
    }

    void DailyPopup::refresh() {
        this->setLoading(true);
        auto url = daily::todayUrl(m_tier);
        m_task.spawn(
            "daily popup",
            [url = std::move(url), agent = std::string(Mod::get()->getID())] {
                return web::WebRequest()
                    .userAgent(agent)
                    .timeout(std::chrono::seconds(30))
                    .get(url);
            },
            [this](web::WebResponse response) {
                auto const error = daily::acceptToday(response);
                if (!error) {
                    daily::resubmitPending();
                }
                this->setLoading(false);
                this->render(error);
            }
        );
    }

    void DailyPopup::render(std::optional<std::string> const& error) {
        this->renderLevel(error);
        this->renderBoard();

        auto const left = daily::secondsLeft();
        m_footer->setString(left ? timeLeft(*left).c_str() : "");
    }

    void DailyPopup::renderLevel(std::optional<std::string> const& error) {
        auto const* tier = daily::tier(m_tier);
        bool const hasLevel = tier && tier->level;
        m_playButton->setVisible(hasLevel);

        for (std::size_t i = 0; i < TABS.size(); ++i) {
            if (m_tier == TABS[i].id) {
                m_tierName->setString(tier ? tier->name.c_str() : TABS[i].label);
            }
        }

        if (m_face) {
            m_face->removeFromParent();
            m_face = nullptr;
        }
        if (auto* face = CCSprite::createWithSpriteFrameName(faceOf(m_tier))) {
            float const size = std::max(face->getContentSize().width, face->getContentSize().height);
            face->setScale(size > 0.f ? std::min(1.f, 46.f / size) : 1.f);
            face->setPosition({ LEVEL_W / 2.f, PANEL_HEIGHT - 52.f });
            m_levelPanel->addChild(face);
            m_face = face;
        }

        if (!hasLevel) {
            m_levelName->setString("");
            m_rules->setString("");
            if (error) {
                m_status->setString(error->c_str());
            }
            else if (m_loading || !daily::today()) {
                m_status->setString("Loading...");
            }
            else {
                m_status->setString("The server could not pick this level yet. Try again in a few minutes");
            }
            return;
        }

        m_levelName->setString(tier->level->name.c_str());
        m_levelName->limitLabelWidth(LEVEL_W - 16.f, 0.5f, 0.2f);

        auto const& today = daily::today();
        bool const roguelike = m_tier == daily::ROGUELIKE;
        bool const boardIsThisTier = today && today->boardTier == m_tier;
        auto const progress = daily::progress(m_tier);

        std::string status;
        if (error) {
            status = *error;
        }
        else if (progress.clearedWith > 0) {
            std::string const rank = boardIsThisTier && today->myRank
                ? fmt::format("\nRank #{} of {}", *today->myRank, today->total) : "";
            status = roguelike ? fmt::format("Best run: {} points{}", progress.score, rank)
                               : fmt::format("Cleared in {} attempts{}", progress.clearedWith, rank);
        }
        else if (progress.baseline >= 0) {
            status = "In progress";
        }
        else {
            status = "Not played yet";
        }
        m_status->setString(status.c_str());

        m_rules->setString(roguelike
            ? "Cards at 25, 50 and 75%. Curses score more. Dying loses them all"
            : "Fewest attempts wins. Practice runs count");
    }

    // 순위표. 순위, 이름, 기록을 칸을 맞춰 한 줄씩 놓는다. 위 세 자리는 금, 은, 동.
    void DailyPopup::renderBoard() {
        m_rows->removeAllChildren();

        auto const& today = daily::today();
        if (m_loading) {
            return;
        }
        if (!today || today->boardTier != m_tier) {
            m_boardEmpty->setString(today ? "" : "No data yet");
            return;
        }
        if (today->top.empty()) {
            m_boardEmpty->setString(m_tier == daily::ROGUELIKE
                ? "No runs yet.\nBe the first!" : "No clears yet.\nBe the first!");
            return;
        }
        m_boardEmpty->setString("");

        bool const scored = m_tier == daily::ROGUELIKE;
        auto* header = label(m_rows, "chatFont.fnt", 0.45f, { BOARD_W - 12.f, PANEL_HEIGHT - 30.f }, { 1.f, 0.5f });
        header->setString(scored ? "Points" : "Attempts");
        header->setOpacity(140);

        constexpr ccColor3B MEDALS[] = { { 255, 210, 60 }, { 210, 220, 235 }, { 230, 150, 90 } };
        for (std::size_t i = 0; i < today->top.size() && i < ROWS; ++i) {
            auto const& row = today->top[i];
            float const y = ROW_TOP - static_cast<float>(i) * ROW_STEP;

            auto* rank = label(m_rows, "goldFont.fnt", 0.42f, { 24.f, y }, { 1.f, 0.5f });
            rank->setString(std::to_string(i + 1).c_str());
            if (i < 3) {
                rank->setColor(MEDALS[i]);
            }

            auto* name = label(m_rows, "chatFont.fnt", 0.55f, { 32.f, y }, { 0.f, 0.5f });
            name->setString(row.name.c_str());
            name->limitLabelWidth(BOARD_W - 100.f, 0.55f, 0.3f);

            auto* value = label(m_rows, "chatFont.fnt", 0.55f, { BOARD_W - 12.f, y }, { 1.f, 0.5f });
            value->setString(scored && row.score ? std::to_string(*row.score).c_str()
                                                 : std::to_string(row.attempts).c_str());
            value->setColor(i < 3 ? MEDALS[i] : ccColor3B{ 255, 255, 255 });
        }

        if (today->total > static_cast<int>(ROWS)) {
            auto* more = label(m_rows, "chatFont.fnt", 0.45f, { BOARD_W / 2.f, 10.f });
            more->setString(fmt::format("{} players today", today->total).c_str());
            more->setOpacity(140);
        }
    }

    void DailyPopup::onTab(CCObject* sender) {
        auto const index = static_cast<std::size_t>(static_cast<CCNode*>(sender)->getTag());
        if (index >= TABS.size() || m_tier == TABS[index].id) {
            return;
        }
        m_tier = TABS[index].id;
        daily::selectTier(m_tier);
        this->paintTabs();
        this->render(std::nullopt);
        this->refresh();
    }

    void DailyPopup::onPlay(CCObject*) {
        daily::open(m_tier);
    }

    void DailyPopup::onRefresh(CCObject*) {
        if (!m_loading) {
            this->refresh();
        }
    }
}
