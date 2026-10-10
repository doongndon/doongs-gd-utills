#include "DailyPopup.hpp"

#include <chrono>

#include <fmt/format.h>

#include "Daily.hpp"

using namespace geode::prelude;

namespace {
    constexpr float WIDTH = 340.f;
    constexpr float HEIGHT = 270.f;
    constexpr float CENTER = WIDTH / 2.f;

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

        m_level = addLabel(m_mainLayer, 212.f, "goldFont.fnt", 0.7f);
        m_status = addLabel(m_mainLayer, 186.f, "chatFont.fnt", 0.6f);
        m_board = addLabel(m_mainLayer, 118.f, "chatFont.fnt", 0.55f);

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

    void DailyPopup::refresh() {
        if (!daily::configured()) {
            this->render(std::string("Set the server address in the mod settings first"));
            return;
        }

        m_status->setString("Loading...");
        auto url = daily::todayUrl();
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
        m_playButton->setVisible(today.has_value());

        if (!today) {
            m_level->setString("");
            m_status->setString(error ? error->c_str() : "Loading...");
            m_board->setString("");
            return;
        }

        m_level->setString(today->levelName.c_str());
        m_level->limitLabelWidth(WIDTH - 40.f, 0.7f, 0.3f);

        auto const& progress = daily::progress();
        std::string status;
        if (error) {
            status = *error;
        }
        else if (progress.clearedWith > 0) {
            status = today->myRank
                ? fmt::format("Cleared in {} attempts - rank #{} of {}",
                              progress.clearedWith, *today->myRank, today->total)
                : fmt::format("Cleared in {} attempts - not sent yet", progress.clearedWith);
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
        if (today->top.empty()) {
            board = "No clears yet - be the first!";
        }
        else {
            for (std::size_t i = 0; i < today->top.size(); ++i) {
                auto const& row = today->top[i];
                fmt::format_to(std::back_inserter(board), "{}. {}  -  {}\n", i + 1, row.name, row.attempts);
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

    void DailyPopup::onPlay(CCObject*) {
        daily::open();
    }

    void DailyPopup::onRefresh(CCObject*) {
        this->refresh();
    }
}
