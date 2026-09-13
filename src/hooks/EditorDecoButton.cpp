#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>

#include "editor/DecoToolsPopup.hpp"
#include "Settings.hpp"

using namespace geode::prelude;

namespace {
    constexpr float PADDING = 6.f;
}

class $modify(DecoEditorUI, EditorUI) {
    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorUI::init(editorLayer)) {
            return false;
        }

        if (!gdu::Settings::get().decoTools) {
            return true;
        }

        auto const size = CCDirector::sharedDirector()->getWinSize();

        auto* sprite = ButtonSprite::create("Deco", "goldFont.fnt", "GJ_button_05.png", 0.7f);
        auto* button = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(DecoEditorUI::onDecoTools)
        );
        button->setID("deco-tools-button"_spr);
        button->setScale(0.6f);

        auto const buttonSize = button->getScaledContentSize();
        button->setPosition({
            size.width - PADDING - buttonSize.width / 2.f,
            size.height - PADDING - buttonSize.height / 2.f
        });

        auto* menu = CCMenu::create();
        menu->setID("deco-tools-menu"_spr);
        menu->setPosition({ 0.f, 0.f });
        menu->addChild(button);
        this->addChild(menu, 100);

        return true;
    }

    void onDecoTools(CCObject*) {
        if (auto* popup = gdu::DecoToolsPopup::create(this)) {
            popup->show();
        }
    }
};
