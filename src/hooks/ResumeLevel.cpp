#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "Session.hpp"
#include "Settings.hpp"

using namespace geode::prelude;

namespace {
    constexpr float PADDING = 8.f;
    constexpr float BUTTON_SCALE = 0.55f;
    constexpr std::size_t NAME_LIMIT = 18;

    std::string shorten(std::string const& name) {
        if (name.size() <= NAME_LIMIT) {
            return name;
        }
        return name.substr(0, NAME_LIMIT - 3) + "...";
    }
}

class $modify(ResumePlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        gdu::session::begin(level);
        return true;
    }

    void togglePracticeMode(bool practiceMode) {
        PlayLayer::togglePracticeMode(practiceMode);
        gdu::session::setPractice(practiceMode);
    }

    void onQuit() {
        // 스스로 나가는 것이므로 "도중에 죽었다" 표시를 지운다.
        gdu::session::end();
        PlayLayer::onQuit();
    }
};

class $modify(ResumeMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) {
            return false;
        }

        if (!gdu::Settings::get().resumeButton) {
            return true;
        }

        auto const record = gdu::session::current();
        if (!record.active || record.levelID <= 0) {
            return true;
        }

        // 저장 목록에 없는 레벨이면 다시 열 방법이 없다. 눌러도 안 되는 버튼을
        // 띄우느니 아예 만들지 않는다.
        if (!GameLevelManager::sharedState()->getSavedLevel(record.levelID)) {
            gdu::session::end();
            return true;
        }

        auto const size = CCDirector::sharedDirector()->getWinSize();

        auto* sprite = ButtonSprite::create("Resume", "goldFont.fnt", "GJ_button_05.png", 0.8f);
        auto* button = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(ResumeMenuLayer::onResumeLevel)
        );
        button->setID("resume-button"_spr);
        button->setScale(BUTTON_SCALE);

        auto const buttonSize = button->getScaledContentSize();
        button->setPosition({
            size.width - PADDING - buttonSize.width / 2.f,
            size.height - PADDING - buttonSize.height / 2.f
        });

        auto* menu = CCMenu::create();
        menu->setID("resume-menu"_spr);
        menu->setPosition({ 0.f, 0.f });
        menu->addChild(button);
        this->addChild(menu, 10);

        auto* label = CCLabelBMFont::create(shorten(record.levelName).c_str(), "chatFont.fnt");
        label->setID("resume-label"_spr);
        label->setAnchorPoint({ 1.f, 1.f });
        label->setScale(0.4f);
        label->setOpacity(160);
        label->setPosition({
            size.width - PADDING,
            size.height - PADDING - buttonSize.height - 2.f
        });
        this->addChild(label, 10);

        return true;
    }

    void onResumeLevel(CCObject*) {
        auto const record = gdu::session::current();
        gdu::session::end();

        auto* level = GameLevelManager::sharedState()->getSavedLevel(record.levelID);
        if (!level) {
            Notification::create("That level is no longer saved", NotificationIcon::Error)->show();
            return;
        }

        // 레벨 데이터가 이미 받아져 있으면 곧장 플레이로 들어간다. 없으면 레벨
        // 페이지로 보내서 GD 가 알아서 내려받게 한다.
        if (level->m_levelString.empty()) {
            CCDirector::sharedDirector()->pushScene(
                CCTransitionFade::create(0.4f, LevelInfoLayer::scene(level, false))
            );
            return;
        }

        CCDirector::sharedDirector()->pushScene(
            CCTransitionFade::create(0.4f, PlayLayer::scene(level, false, false))
        );

        if (!record.practice) {
            return;
        }

        // 씬이 실제로 돌기 시작한 다음 프레임에 연습 모드를 켠다. 아직 화면에
        // 올라가지도 않은 레이어를 건드리면 깨지기 쉽다.
        Loader::get()->queueInMainThread([] {
            if (auto* playLayer = PlayLayer::get()) {
                playLayer->togglePracticeMode(true);
            }
        });
    }
};
