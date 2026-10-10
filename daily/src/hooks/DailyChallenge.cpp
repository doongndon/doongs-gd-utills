#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>

#include "../Art.hpp"
#include "../Daily.hpp"
#include "../ui/DailyPopup.hpp"

using namespace geode::prelude;

class $modify(DailyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) {
            return false;
        }
        // 메뉴에서 곧장 레벨에 들어가도 오늘의 도전인지 알아보도록 미리 받아 둔다.
        gdu::daily::refreshInBackground();

        // 메뉴 아래쪽 줄의 다른 단추(업적, 설정, 통계)와 같은 크기의 둥근 단추. 크기를
        // 따로 줄이지 않는다. 줄인 단추는 누를 때 원래 크기를 기준으로 커져 튀어 보인다.
        CircleButtonSprite* sprite = nullptr;
        if (auto* icon = gdu::art::sprite("menu-icon.png")) {
            sprite = CircleButtonSprite::create(icon, CircleBaseColor::Green, CircleBaseSize::MediumAlt);
        }
        if (!sprite) {
            sprite = CircleButtonSprite::createWithSpriteFrameName(
                "GJ_timeIcon_001.png", 1.1f, CircleBaseColor::Green, CircleBaseSize::MediumAlt
            );
        }
        if (!sprite) {
            return true;
        }

        // 오늘 아직 아무 난이도도 깨지 않았으면 단추 귀퉁이에 빨간 점이 뛴다.
        if (auto* badge = gdu::art::sprite("badge.png")) {
            gdu::art::fit(badge, 14.f);
            float const scale = badge->getScale();
            auto const size = sprite->getContentSize();
            badge->setPosition({ size.width - 7.f, size.height - 7.f });
            badge->setVisible(false);
            sprite->addChild(badge, 10);
            badge->runAction(CCRepeatForever::create(CCSequence::create(
                CallFuncExt::create([badge] {
                    badge->setVisible(gdu::daily::today() && !gdu::daily::clearedAnyToday());
                }),
                CCDelayTime::create(1.f),
                nullptr
            )));
            badge->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCScaleTo::create(0.6f, scale * 1.18f)),
                CCEaseSineInOut::create(CCScaleTo::create(0.6f, scale)),
                nullptr
            )));
        }

        auto* button = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(DailyMenuLayer::onDailyChallenge)
        );
        button->setID("daily-challenge-button"_spr);

        // 아래쪽 단추 줄이 있으면 거기에 끼운다. 다른 모드가 그 줄을 옮기거나
        // 없앴으면 왼쪽 가운데에 따로 둔다.
        if (auto* bottom = this->getChildByID("bottom-menu")) {
            bottom->addChild(button);
            bottom->updateLayout();
            return true;
        }

        auto const winSize = CCDirector::sharedDirector()->getWinSize();
        auto const half = button->getContentSize() / 2.f;
        button->setPosition({ 12.f + half.width, winSize.height / 2.f });

        auto* menu = CCMenu::create();
        menu->setID("daily-challenge-menu"_spr);
        menu->setPosition({ 0.f, 0.f });
        menu->addChild(button);
        this->addChild(menu, 10);
        return true;
    }

    void onDailyChallenge(CCObject*) {
        if (auto* popup = gdu::ui::DailyPopup::create()) {
            popup->show();
        }
    }
};

class $modify(DailyPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        // 시도 수를 세기 시작하는 기준은 레벨을 열기 전의 값이어야 한다. 열면서
        // GD 가 첫 시도를 하나 더하기 때문이다.
        gdu::daily::startPlaying(level);
        return PlayLayer::init(level, useReplay, dontCreateObjects);
    }
};
