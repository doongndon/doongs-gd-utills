#include <Geode/Geode.hpp>
#include <Geode/modify/DailyLevelNode.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/modify/LevelCell.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/LevelListCell.hpp>
#include <Geode/modify/LevelPage.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/TextGameObject.hpp>

#include <string>

#include "KoreanFont.hpp"

using namespace geode::prelude;

// 레벨 하나를 그리는 동안 그 레벨의 이름을 번역하지 않게 한다. KoreanFont.hpp 를
// 보라. 다른 모드도 이 함수들에 훅을 걸어 이름 라벨을 덧붙이므로(Overcharged
// Levels 의 레벨 쪽 이름 상자처럼), 우리 훅은 맨 바깥에서 그 모두를 감싼다.

namespace {
    std::string nameOf(GJGameLevel* level) {
        return level ? std::string(level->m_levelName) : std::string();
    }

    std::string nameOfPlaying() {
        auto* play = PlayLayer::get();
        return play ? nameOf(play->m_level) : std::string();
    }
}

class $modify(KoreanNameLevelCell, LevelCell) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("LevelCell::loadFromLevel", Priority::First);
    }

    void loadFromLevel(GJGameLevel* level) {
        kopatch::LevelNameScope const scope(nameOf(level));
        LevelCell::loadFromLevel(level);
    }
};

class $modify(KoreanNameLevelListCell, LevelListCell) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("LevelListCell::loadFromList", Priority::First);
    }

    void loadFromList(GJLevelList* list) {
        kopatch::LevelNameScope const scope(list ? std::string(list->m_listName) : std::string());
        LevelListCell::loadFromList(list);
    }
};

class $modify(KoreanNameLevelInfoLayer, LevelInfoLayer) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("LevelInfoLayer::init", Priority::First);
        (void)self.setHookPriority("LevelInfoLayer::levelDownloadFinished", Priority::First);
        (void)self.setHookPriority("LevelInfoLayer::updateLabelValues", Priority::First);
    }

    bool init(GJGameLevel* level, bool challenge) {
        kopatch::LevelNameScope const scope(nameOf(level));
        return LevelInfoLayer::init(level, challenge);
    }

    void levelDownloadFinished(GJGameLevel* level) {
        kopatch::LevelNameScope const scope(nameOf(level));
        LevelInfoLayer::levelDownloadFinished(level);
    }

    void updateLabelValues() {
        kopatch::LevelNameScope const scope(nameOf(m_level));
        LevelInfoLayer::updateLabelValues();
    }
};

class $modify(KoreanNameEditLevelLayer, EditLevelLayer) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("EditLevelLayer::init", Priority::First);
    }

    bool init(GJGameLevel* level) {
        kopatch::LevelNameScope const scope(nameOf(level));
        return EditLevelLayer::init(level);
    }
};

class $modify(KoreanNameLevelPage, LevelPage) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("LevelPage::init", Priority::First);
        (void)self.setHookPriority("LevelPage::updateDynamicPage", Priority::First);
    }

    bool init(GJGameLevel* level) {
        kopatch::LevelNameScope const scope(nameOf(level));
        return LevelPage::init(level);
    }

    void updateDynamicPage(GJGameLevel* level) {
        kopatch::LevelNameScope const scope(nameOf(level));
        LevelPage::updateDynamicPage(level);
    }
};

class $modify(KoreanNameDailyLevelNode, DailyLevelNode) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("DailyLevelNode::init", Priority::First);
    }

    bool init(GJGameLevel* level, DailyLevelPage* page, bool isNew) {
        kopatch::LevelNameScope const scope(nameOf(level));
        return DailyLevelNode::init(level, page, isNew);
    }
};

class $modify(KoreanNamePauseLayer, PauseLayer) {
    static void onModify(auto& self) {
        (void)self.setHookPriority("PauseLayer::customSetup", Priority::First);
    }

    void customSetup() {
        kopatch::LevelNameScope const scope(nameOfPlaying());
        PauseLayer::customSetup();
    }
};

// 레벨 안에 놓인 글자 오브젝트는 만든 사람이 쓴 글이다. 레벨 이름과 같은 까닭으로
// 번역하지 않는다. "Progress" 라고 써 둔 표지판이 "진행도" 가 되면 안 된다.
// 쪼개는 동안 라벨 훅을 재우는 깃발을 그대로 빌려 쓴다.
class $modify(KoreanKeepTextObject, TextGameObject) {
    void updateTextObject(gd::string text, bool defaultFont) {
        bool const was = kopatch::splittingText();
        kopatch::setSplittingText(true);
        TextGameObject::updateTextObject(text, defaultFont);
        kopatch::setSplittingText(was);
    }
};
