#include "CardInfo.hpp"

#include "../Text.hpp"
#include "CardFace.hpp"

using namespace geode::prelude;

namespace gdu::ui {
    CardInfo* CardInfo::create(std::string const& id) {
        auto* popup = new CardInfo();
        if (popup->init(id)) {
            popup->autorelease();
            return popup;
        }
        delete popup;
        return nullptr;
    }

    bool CardInfo::init(std::string const& id) {
        auto const size = CCDirector::sharedDirector()->getWinSize();
        if (!Popup::init(size.width, size.height)) {
            return false;
        }
        m_noElasticity = true;
        m_bgSprite->setVisible(false);
        m_closeBtn->setVisible(false);

        // 화면 전체가 닫기 단추다.
        auto* anywhere = CCMenuItemExt::create([this](CCMenuItem*) { this->onClose(nullptr); });
        anywhere->setContentSize(size);
        anywhere->setAnchorPoint({ 0.5f, 0.5f });
        anywhere->setPosition(size / 2.f);
        m_buttonMenu->addChild(anywhere, -1);

        auto* face = CardFace::create(id);
        if (!face) {
            return false;
        }
        face->showFront(true);
        face->setPosition({ size.width / 2.f, size.height / 2.f + 8.f });
        m_mainLayer->addChild(face);
        m_card = face;

        auto* hint = text::label(std::string(text::tr("info.close")), text::Font::Chat, 0.55f);
        hint->setOpacity(170);
        hint->setPosition({ size.width / 2.f, 22.f });
        m_mainLayer->addChild(hint);
        return true;
    }

    void CardInfo::show() {
        Popup::show();
        m_mainLayer->stopAllActions();
        m_mainLayer->setScale(1.f);
        this->stopAllActions();
        this->setOpacity(0);
        this->runAction(CCFadeTo::create(0.15f, 170));
        if (m_card) {
            m_card->setScale(0.6f);
            m_card->runAction(CCEaseBackOut::create(CCScaleTo::create(0.25f, 1.3f)));
        }
    }
}
