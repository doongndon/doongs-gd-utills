#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "Settings.hpp"
#include "daily/Daily.hpp"
#include "daily/DailyPopup.hpp"

using namespace geode::prelude;

class $modify(DailyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) {
            return false;
        }
        if (!gdu::Settings::get().dailyEnabled) {
            return true;
        }

        // 메뉴에서 곧장 레벨에 들어가도 오늘의 도전인지 알아보도록 미리 받아 둔다.
        gdu::daily::refreshInBackground();

        auto* sprite = ButtonSprite::create("Daily", "goldFont.fnt", "GJ_button_01.png", 0.8f);
        auto* button = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(DailyMenuLayer::onDailyChallenge)
        );
        button->setID("daily-challenge-button"_spr);

        // 아래쪽 단추 줄이 있으면 거기에 끼운다. 다른 모드가 그 줄을 옮기거나
        // 없앴으면 왼쪽 아래에 따로 둔다.
        if (auto* bottom = this->getChildByID("bottom-menu")) {
            button->setScale(0.7f);
            bottom->addChild(button);
            bottom->updateLayout();
            return true;
        }

        auto const size = CCDirector::sharedDirector()->getWinSize();
        button->setScale(0.6f);
        auto const half = button->getScaledContentSize() / 2.f;
        button->setPosition({ 8.f + half.width, size.height / 2.f });

        auto* menu = CCMenu::create();
        menu->setID("daily-challenge-menu"_spr);
        menu->setPosition({ 0.f, 0.f });
        menu->addChild(button);
        this->addChild(menu, 10);
        return true;
    }

    void onDailyChallenge(CCObject*) {
        if (auto* popup = gdu::DailyPopup::create()) {
            popup->show();
        }
    }
};

class $modify(DailyPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        // 시도 수를 세기 시작하는 기준은 레벨을 열기 전의 값이어야 한다. 열면서
        // GD 가 첫 시도를 하나 더하기 때문이다.
        if (gdu::Settings::get().dailyEnabled) {
            gdu::daily::startPlaying(level);
        }
        return PlayLayer::init(level, useReplay, dontCreateObjects);
    }

    void levelComplete() {
        PlayLayer::levelComplete();

        // 연습 모드나 시작 위치로 깬 것은 깬 것으로 치지 않는다.
        if (!gdu::Settings::get().dailyEnabled || m_isPracticeMode || m_isTestMode) {
            return;
        }
        gdu::daily::cleared(m_level);
    }
};
