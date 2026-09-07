#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>

#include <fmt/format.h>

#include "Settings.hpp"

using namespace geode::prelude;

namespace {
    constexpr float PADDING = 10.f;
}

class $modify(LevelInfoPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        if (!gdu::Settings::get().pauseInfo) {
            return;
        }

        auto* playLayer = PlayLayer::get();
        if (!playLayer || !playLayer->m_level) {
            return;
        }

        auto* level = playLayer->m_level;
        int const levelID = level->m_levelID.value();

        std::string info;
        info.reserve(64);
        if (levelID > 0) {
            fmt::format_to(std::back_inserter(info), "ID {}\n", levelID);
        }
        fmt::format_to(
            std::back_inserter(info), "Objects {}\nAttempts {}",
            level->m_objectCount.value(), level->m_attempts.value()
        );

        auto* label = CCLabelBMFont::create(info.c_str(), "chatFont.fnt");
        label->setID("level-info"_spr);
        label->setAnchorPoint({ 0.f, 0.f });
        label->setScale(0.5f);
        label->setOpacity(180);
        label->setPosition({ PADDING, PADDING });
        this->addChild(label, 10);

        if (levelID <= 0) {
            return;
        }

        auto* menu = CCMenu::create();
        menu->setID("level-info-menu"_spr);
        menu->setPosition({ 0.f, 0.f });

        auto* sprite = ButtonSprite::create("Copy ID", "goldFont.fnt", "GJ_button_05.png", 0.8f);

        auto* button = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(LevelInfoPauseLayer::onCopyLevelID)
        );
        button->setID("copy-id-button"_spr);
        button->setScale(0.6f);
        button->setPosition({
            PADDING + button->getScaledContentSize().width / 2.f,
            PADDING + label->getScaledContentSize().height + 16.f
        });
        menu->addChild(button);

        this->addChild(menu, 10);
    }

    void onCopyLevelID(CCObject*) {
        auto* playLayer = PlayLayer::get();
        if (!playLayer || !playLayer->m_level) {
            return;
        }

        auto const id = std::to_string(playLayer->m_level->m_levelID.value());
        if (PlatformToolbox::copyToClipboard(id)) {
            Notification::create("Level ID copied", NotificationIcon::Success)->show();
        }
        else {
            Notification::create("Failed to copy level ID", NotificationIcon::Error)->show();
        }
    }
};
