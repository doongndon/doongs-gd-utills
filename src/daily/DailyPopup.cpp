#include "DailyPopup.hpp"

#include <algorithm>
#include <array>
#include <chrono>

#include <fmt/format.h>

#include "Daily.hpp"

using namespace geode::prelude;

namespace {
    constexpr float WIDTH = 360.f;
    constexpr float HEIGHT = 290.f;
    constexpr float CENTER = WIDTH / 2.f;

    struct TierButton {
        char const* id;
        char const* label;
    };

    // 서버의 난이도 목록(daily-server/server.js 의 TIERS)과 같은 차례다. 서버의 답이
    // 오기 전에도 단추를 그릴 수 있게 여기에도 적어 둔다.
    constexpr std::array<TierButton, 8> TIER_BUTTONS = {{
        { "main", "Main" },
        { "easy", "Easy" },
        { "normal", "Normal" },
        { "hard", "Hard" },
        { "harder", "Harder" },
        { "insane", "Insane" },
        { "easy-demon", "Demon" },
        { "roguelike", "Rogue" },
    }};

    CCLabelBMFont* addLabel(CCNode* parent, float y, char const* font, float scale) {
        auto* label = CCLabelBMFont::create("", font);
        label->setScale(scale);
        label->setPosition({ CENTER, y });
        parent->addChild(label);
        return label;
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
        this->setTitle("Today's Challenge");

        m_tier = daily::selectedTier();

        m_tierMenu = CCMenu::create();
        m_tierMenu->setPosition({ 0.f, 0.f });
        m_mainLayer->addChild(m_tierMenu);
        this->buildTierRow();

        m_level = addLabel(m_mainLayer, 208.f, "goldFont.fnt", 0.7f);
        m_status = addLabel(m_mainLayer, 184.f, "chatFont.fnt", 0.6f);
        m_board = addLabel(m_mainLayer, 112.f, "chatFont.fnt", 0.55f);

        auto* play = ButtonSprite::create("Play", "goldFont.fnt", "GJ_button_01.png", 0.8f);
        m_playButton = CCMenuItemSpriteExtra::create(play, this, menu_selector(DailyPopup::onPlay));
        m_playButton->setScale(0.8f);
        m_playButton->setPosition({ CENTER - 55.f, 30.f });
        m_buttonMenu->addChild(m_playButton);

        auto* again = ButtonSprite::create("Refresh", "goldFont.fnt", "GJ_button_04.png", 0.8f);
        auto* refreshButton = CCMenuItemSpriteExtra::create(again, this, menu_selector(DailyPopup::onRefresh));
        refreshButton->setScale(0.8f);
        refreshButton->setPosition({ CENTER + 55.f, 30.f });
        m_buttonMenu->addChild(refreshButton);

        // 받아 둔 것이 있으면 먼저 보여 주고, 뒤에서 새로 받아 바꾼다.
        this->render(std::nullopt);
        this->refresh();
        return true;
    }

    // 난이도 단추 줄. 처음 한 번만 짓고, 고를 때는 배경만 바꿔 칠한다. 누른 단추를
    // 그 단추의 콜백 안에서 지웠다 다시 지으면 아직 돌고 있는 단추를 지우는 셈이 된다.
    void DailyPopup::buildTierRow() {
        float const slot = (WIDTH - 24.f) / static_cast<float>(TIER_BUTTONS.size());
        for (std::size_t i = 0; i < TIER_BUTTONS.size(); ++i) {
            auto* sprite = ButtonSprite::create(
                TIER_BUTTONS[i].label, "bigFont.fnt", "GJ_button_04.png", 0.6f);
            auto* button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(DailyPopup::onTier));
            button->setTag(static_cast<int>(i));

            float const width = button->getContentSize().width;
            button->setScale(width > 0.f ? std::min(0.6f, (slot - 4.f) / width) : 0.6f);
            button->setPosition({ 12.f + slot * (static_cast<float>(i) + 0.5f), 238.f });
            m_tierMenu->addChild(button);
            m_tierSprites.push_back(sprite);
        }
        this->paintTierRow();
    }

    // 고른 난이도는 초록 단추로 그린다.
    void DailyPopup::paintTierRow() {
        for (std::size_t i = 0; i < m_tierSprites.size() && i < TIER_BUTTONS.size(); ++i) {
            m_tierSprites[i]->updateBGImage(
                m_tier == TIER_BUTTONS[i].id ? "GJ_button_01.png" : "GJ_button_04.png");
        }
    }

    void DailyPopup::refresh() {
        if (!daily::configured()) {
            this->render(std::string("Set the server address in the mod settings first"));
            return;
        }

        m_status->setString("Loading...");
        auto url = daily::todayUrl(m_tier);
        m_task.spawn(
            "daily popup",
            [url = std::move(url), agent = std::string(Mod::get()->getID())] {
                return web::WebRequest()
                    .userAgent(agent)
                    .timeout(std::chrono::seconds(20))
                    .get(url);
            },
            [this](web::WebResponse response) {
                auto const error = daily::acceptToday(response);
                if (!error) {
                    daily::resubmitPending();
                }
                this->render(error);
            }
        );
    }

    void DailyPopup::render(std::optional<std::string> const& error) {
        auto const& today = daily::today();
        auto const* tier = daily::tier(m_tier);
        bool const hasLevel = tier && tier->level;
        m_playButton->setVisible(hasLevel);

        if (!today || !tier) {
            m_level->setString("");
            m_status->setString(error ? error->c_str() : "Loading...");
            m_board->setString("");
            return;
        }

        if (!hasLevel) {
            m_level->setString(tier->name.c_str());
            m_status->setString(error ? error->c_str()
                                      : "The server could not pick this level yet - try again later");
            m_status->limitLabelWidth(WIDTH - 30.f, 0.6f, 0.3f);
            m_board->setString("");
            return;
        }

        m_level->setString(fmt::format("{} - {}", tier->name, tier->level->name).c_str());
        m_level->limitLabelWidth(WIDTH - 40.f, 0.7f, 0.3f);

        // 순위표는 고른 난이도의 것일 때만 보여 준다. 다른 난이도의 답이 늦게 오면
        // 잠깐 엇갈릴 수 있다.
        bool const boardIsThisTier = today->boardTier == m_tier;

        auto const progress = daily::progress(m_tier);
        bool const roguelike = m_tier == daily::ROGUELIKE;
        std::string status;
        if (error) {
            status = *error;
        }
        else if (roguelike && progress.clearedWith > 0) {
            status = boardIsThisTier && today->myRank
                ? fmt::format("Best run: {} points - rank #{} of {}",
                              progress.score, *today->myRank, today->total)
                : fmt::format("Best run: {} points", progress.score);
        }
        else if (roguelike) {
            status = "Pick a card at 25, 50 and 75%. Curses score more - die and you lose them all";
        }
        else if (progress.clearedWith > 0) {
            status = boardIsThisTier && today->myRank
                ? fmt::format("Cleared in {} attempts - rank #{} of {}",
                              progress.clearedWith, *today->myRank, today->total)
                : fmt::format("Cleared in {} attempts", progress.clearedWith);
        }
        else if (progress.baseline >= 0) {
            status = "In progress - the attempts since you first opened it count";
        }
        else {
            status = "Fewest attempts to beat it wins. Practice runs count too";
        }
        m_status->setString(status.c_str());
        m_status->limitLabelWidth(WIDTH - 30.f, 0.6f, 0.3f);

        std::string board;
        if (!boardIsThisTier) {
            board = "Loading...";
        }
        else if (today->top.empty()) {
            board = "No clears yet - be the first!";
        }
        else {
            for (std::size_t i = 0; i < today->top.size(); ++i) {
                auto const& row = today->top[i];
                if (row.score) {
                    fmt::format_to(std::back_inserter(board), "{}. {}  -  {} pts ({} tries)\n",
                                   i + 1, row.name, *row.score, row.attempts);
                }
                else {
                    fmt::format_to(std::back_inserter(board), "{}. {}  -  {}\n", i + 1, row.name, row.attempts);
                }
            }
            if (today->total > static_cast<int>(today->top.size())) {
                fmt::format_to(std::back_inserter(board), "... {} players today", today->total);
            }
            else if (!board.empty()) {
                board.pop_back();
            }
        }
        m_board->setString(board.c_str());
    }

    void DailyPopup::onTier(CCObject* sender) {
        auto const index = static_cast<std::size_t>(static_cast<CCNode*>(sender)->getTag());
        if (index >= TIER_BUTTONS.size() || m_tier == TIER_BUTTONS[index].id) {
            return;
        }
        m_tier = TIER_BUTTONS[index].id;
        daily::selectTier(m_tier);
        this->paintTierRow();
        this->render(std::nullopt);
        this->refresh();
    }

    void DailyPopup::onPlay(CCObject*) {
        daily::open(m_tier);
    }

    void DailyPopup::onRefresh(CCObject*) {
        this->refresh();
    }
}
