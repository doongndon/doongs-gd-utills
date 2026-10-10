#include "CardPicker.hpp"

#include <algorithm>
#include <cctype>

#include <fmt/format.h>

#include "../Art.hpp"
#include "../Daily.hpp"
#include "../Text.hpp"
#include "CardFace.hpp"

using namespace geode::prelude;

namespace {
    using gdu::ui::CardFace;
    using gdu::text::Font;

    // 고르는 동안의 배경 어둡기. 카운트다운 때는 판이 보여야 준비할 수 있으므로 옅게.
    constexpr GLubyte DIM = 175;
    constexpr GLubyte DIM_COUNTDOWN = 60;

    constexpr float GAP = 16.f;
    constexpr float CARDS_Y = -8.f;

    // 깔기와 뒤집기의 박자(초).
    constexpr float DEAL = 0.34f;
    constexpr float DEAL_STAGGER = 0.08f;
    constexpr float FLIP_START = 0.45f;
    constexpr float FLIP_STAGGER = 0.12f;
    constexpr float FLIP_HALF = 0.09f;

    // 고른 뒤의 박자. 고른 순간부터 GO 까지 3.5초.
    constexpr float SHOWCASE = 0.45f;
    constexpr float FLIGHT = 0.38f;
    constexpr float BEAT = 1.f;

    CCLabelBMFont* label(CCNode* parent, std::string const& text, Font font, float scale, CCPoint position, int z = 5) {
        auto* node = gdu::text::label(text, font, scale);
        node->setPosition(position);
        parent->addChild(node, z);
        return node;
    }

    std::string tr(char const* key) {
        return std::string(gdu::text::tr(key));
    }
}

namespace gdu::ui {
    CardPicker* CardPicker::create(int percent, std::vector<std::string> const& offer, Hooks hooks) {
        auto* picker = new CardPicker();
        if (picker->init(percent, offer, std::move(hooks))) {
            picker->autorelease();
            return picker;
        }
        delete picker;
        return nullptr;
    }

    bool CardPicker::init(int percent, std::vector<std::string> const& offer, Hooks hooks) {
        auto const size = CCDirector::sharedDirector()->getWinSize();
        if (offer.empty() || !Popup::init(size.width, size.height)) {
            return false;
        }
        m_hooks = std::move(hooks);
        m_percent = percent;
        m_offer = offer;
        // Popup 이 뜰 때 튀어나오는 움직임은 쓰지 않는다. 카드가 따로 움직인다.
        m_noElasticity = true;
        this->setID("card-picker"_spr);

        // Popup 의 판과 닫기 단추는 쓰지 않는다. 고르기 전에는 닫을 수 없다.
        m_bgSprite->setVisible(false);
        m_closeBtn->setVisible(false);
        m_closeBtn->setEnabled(false);
        // 깔리고 뒤집히는 동안에는 고를 수 없다.
        m_buttonMenu->setEnabled(false);
        this->setKeyboardEnabled(true);

        float const w = size.width;
        float const h = size.height;
        m_title = label(m_mainLayer, fmt::format("{}%", percent), Font::Big, 0.95f, { w / 2.f, h - 30.f });
        m_subtitle = label(m_mainLayer, tr("picker.choose"), Font::Gold, 0.65f, { w / 2.f, h - 56.f });
        text::fit(m_subtitle, Font::Gold, w - 40.f, 0.65f, 0.2f);

#ifdef GEODE_IS_DESKTOP
        auto const hint = tr("picker.hintDesktop");
#else
        auto const hint = tr("picker.hintMobile");
#endif
        m_hint = label(m_mainLayer, hint, Font::Chat, 0.6f, { w / 2.f, 50.f });
        text::fit(m_hint, Font::Chat, w - 40.f, 0.6f, 0.2f);
        m_rule = label(m_mainLayer, tr("picker.rule"), Font::Chat, 0.5f, { w / 2.f, 34.f });
        text::fit(m_rule, Font::Chat, w - 40.f, 0.5f, 0.2f);
        for (auto* node : { m_hint, m_rule }) {
            node->setOpacity(0);
        }

        float const step = CardFace::WIDTH + GAP;
        float const first = w / 2.f - step * static_cast<float>(offer.size() - 1) / 2.f;
        for (std::size_t i = 0; i < offer.size(); ++i) {
            // 단추 안에 카드를 담는 판을 둔다. 카드가 떠다니고 뒤집혀도 누르는 자리는 그대로다.
            auto* holder = CCNode::create();
            holder->setContentSize({ CardFace::WIDTH, CardFace::HEIGHT });

            auto* face = CardFace::create(offer[i]);
            if (!face) {
                return false;
            }
            face->setPosition({ CardFace::WIDTH / 2.f, CardFace::HEIGHT / 2.f });
            face->showFront(false);
            holder->addChild(face, 1);

            CCSprite* glow = art::sprite("glow.png");
            if (glow) {
                art::fit(glow, CardFace::HEIGHT * 1.45f);
                glow->setColor(art::kindLight(face->isHelp()));
                art::additive(glow);
                glow->setOpacity(0);
                glow->setPosition({ CardFace::WIDTH / 2.f, CardFace::HEIGHT / 2.f });
                holder->addChild(glow, 0);
            }

            auto* button = CCMenuItemSpriteExtra::create(holder, this, menu_selector(CardPicker::onCard));
            button->setTag(static_cast<int>(i));
            // 카드는 크므로 누를 때 아주 조금만 커지게 한다(기본 1.26배).
            button->m_scaleMultiplier = 1.04f;
            button->setPosition({ first + step * static_cast<float>(i), -CardFace::HEIGHT });
            m_buttonMenu->addChild(button);

            m_buttons.push_back(button);
            m_faces.push_back(face);
            m_glows.push_back(glow);
        }

        m_count = label(m_mainLayer, "", Font::Big, 1.7f, { w / 2.f, h / 2.f + 12.f }, 30);
        return true;
    }

    void CardPicker::show() {
        Popup::show();
        // FLAlertLayer 가 뜨면서 거는 움직임을 걷고 우리 것으로 바꾼다.
        this->stopAllActions();
        m_mainLayer->stopAllActions();
        m_mainLayer->setScale(1.f);
        this->setOpacity(0);
        this->runAction(CCFadeTo::create(0.25f, DIM));
        art::play("sfx-deal.ogg", 0.5f);

        auto const size = CCDirector::sharedDirector()->getWinSize();
        for (auto* node : { m_title, m_subtitle }) {
            auto const end = node->getPosition();
            node->setPosition(end + CCPoint{ 0.f, 30.f });
            node->setOpacity(0);
            node->runAction(CCSpawn::create(
                CCEaseBackOut::create(CCMoveTo::create(0.35f, end)),
                CCFadeIn::create(0.25f),
                nullptr
            ));
        }

        for (std::size_t i = 0; i < m_buttons.size(); ++i) {
            CCPoint const end{ m_buttons[i]->getPositionX(), size.height / 2.f + CARDS_Y };
            m_buttons[i]->runAction(CCSequence::create(
                CCDelayTime::create(DEAL_STAGGER * static_cast<float>(i)),
                CCEaseBackOut::create(CCMoveTo::create(DEAL, end)),
                nullptr
            ));
            this->runAction(CCSequence::create(
                CCDelayTime::create(FLIP_START + FLIP_STAGGER * static_cast<float>(i)),
                CallFuncExt::create([this, i] { this->flip(i); }),
                nullptr
            ));
        }
        float const settled = FLIP_START + FLIP_STAGGER * static_cast<float>(m_buttons.size()) + FLIP_HALF * 2.f;
        this->runAction(CCSequence::create(
            CCDelayTime::create(settled),
            CallFuncExt::create([this] { this->ready(); }),
            nullptr
        ));
    }

    void CardPicker::flip(std::size_t index) {
        if (index >= m_faces.size()) {
            return;
        }
        auto* face = m_faces[index];
        art::play("sfx-flip.ogg", 0.45f);
        face->runAction(CCSequence::create(
            CCEaseIn::create(CCScaleTo::create(FLIP_HALF, 0.f, 1.f), 2.f),
            CallFuncExt::create([face] { face->showFront(true); }),
            CCEaseOut::create(CCScaleTo::create(FLIP_HALF, 1.f, 1.f), 2.f),
            nullptr
        ));
    }

    void CardPicker::ready() {
        if (m_picked) {
            return;
        }
        m_ready = true;
        m_buttonMenu->setEnabled(true);
        m_hint->runAction(CCFadeTo::create(0.25f, 230));
        m_rule->runAction(CCFadeTo::create(0.25f, 150));

        // 살짝 떠다니는 카드. 한 장씩 박자를 어긋나게 한다.
        for (std::size_t i = 0; i < m_faces.size(); ++i) {
            float const lift = 2.5f;
            m_faces[i]->runAction(CCSequence::create(
                CCDelayTime::create(0.18f * static_cast<float>(i)),
                CallFuncExt::create([face = m_faces[i], lift] {
                    face->runAction(CCRepeatForever::create(CCSequence::create(
                        CCEaseSineInOut::create(CCMoveBy::create(1.1f, { 0.f, lift })),
                        CCEaseSineInOut::create(CCMoveBy::create(1.1f, { 0.f, -lift })),
                        nullptr
                    )));
                }),
                nullptr
            ));
        }
    }

    void CardPicker::focus(int index) {
        if (m_faces.empty()) {
            return;
        }
        m_focus = std::clamp(index, 0, static_cast<int>(m_faces.size()) - 1);
        for (std::size_t i = 0; i < m_faces.size(); ++i) {
            bool const on = static_cast<int>(i) == m_focus;
            if (m_glows[i]) {
                m_glows[i]->stopAllActions();
                m_glows[i]->runAction(CCFadeTo::create(0.12f, on ? 210 : 0));
            }
            // 떠다니는 움직임과 겹치지 않게 단추 쪽을 키운다.
            m_buttons[i]->stopActionByTag(7);
            auto* grow = CCEaseOut::create(CCScaleTo::create(0.12f, on ? 1.06f : 1.f), 2.f);
            grow->setTag(7);
            m_buttons[i]->runAction(grow);
            m_buttons[i]->m_baseScale = on ? 1.06f : 1.f;
        }
    }

    void CardPicker::onCard(CCObject* sender) {
        auto* node = static_cast<CCNode*>(sender);
        if (node) {
            this->pick(static_cast<std::size_t>(node->getTag()));
        }
    }

    void CardPicker::pick(std::size_t index) {
        if (!m_ready || m_picked || index >= m_offer.size()) {
            return;
        }
        m_picked = true;
        m_buttonMenu->setEnabled(false);

        auto const id = m_offer[index];
        auto const* card = daily::card(id);
        bool const help = card && card->help;
        int const points = card ? card->points : 0;
        art::play("sfx-pick.ogg", 0.7f);
        if (m_hooks.picked) {
            m_hooks.picked(id);
        }

        // 고르지 않은 카드는 가라앉는다.
        for (std::size_t i = 0; i < m_buttons.size(); ++i) {
            if (i == index) {
                continue;
            }
            m_buttons[i]->stopAllActions();
            m_buttons[i]->runAction(CCSequence::create(
                CCSpawn::create(
                    CCEaseIn::create(CCMoveBy::create(0.25f, { 0.f, -36.f }), 2.f),
                    CCScaleTo::create(0.25f, 0.82f),
                    nullptr
                ),
                CCHide::create(),
                nullptr
            ));
        }

        // 고른 카드는 단추에서 꺼내 따로 띄운다. 이 카드가 HUD 로 날아간다.
        auto* chosen = m_buttons[index];
        CCPoint const at = chosen->getPosition();
        chosen->setVisible(false);

        if (auto* burst = CCSprite::createWithSpriteFrameName("shineBurst_001.png")) {
            burst->setPosition(at);
            burst->setColor(art::kindLight(help));
            art::additive(burst);
            burst->setScale(0.2f);
            burst->setOpacity(0);
            burst->runAction(CCRepeatForever::create(CCRotateBy::create(4.f, 360.f)));
            burst->runAction(CCSpawn::create(
                CCEaseBackOut::create(CCScaleTo::create(0.3f, 2.4f)),
                CCFadeTo::create(0.2f, 200),
                nullptr
            ));
            m_mainLayer->addChild(burst, 19);
            m_burst = burst;
        }

        if (auto* flying = CardFace::create(id)) {
            flying->showFront(true);
            flying->setPosition(at);
            flying->runAction(CCEaseBackOut::create(CCScaleTo::create(0.2f, 1.14f)));
            m_mainLayer->addChild(flying, 20);
            m_flying = flying;
        }

        auto const title = text::upper(text::cardName(id, card ? card->name : id));
        auto const size = CCDirector::sharedDirector()->getWinSize();
        m_title->stopAllActions();
        text::setFit(m_title, title, Font::Big, size.width - 40.f, 0.95f, 0.3f);
        float const titleScale = m_title->getScale();
        m_title->setColor(art::kindLight(help));
        m_title->setOpacity(255);
        m_title->setScale(titleScale * 1.3f);
        m_title->runAction(CCEaseBackOut::create(CCScaleTo::create(0.25f, titleScale)));
        m_subtitle->stopAllActions();
        m_subtitle->setOpacity(255);
        text::set(m_subtitle, text::format("picker.points", art::pointsText(points)), Font::Gold, 0.65f);
        m_subtitle->setColor(art::pointsColor(points));
        for (auto* node : { m_hint, m_rule }) {
            node->stopAllActions();
            node->runAction(CCFadeOut::create(0.15f));
        }

        this->runAction(CCSequence::create(
            CCDelayTime::create(SHOWCASE),
            CallFuncExt::create([this] { this->fly(); }),
            CCDelayTime::create(0.05f),
            CallFuncExt::create([this] { this->count(3); }),
            CCDelayTime::create(BEAT),
            CallFuncExt::create([this] { this->count(2); }),
            CCDelayTime::create(BEAT),
            CallFuncExt::create([this] { this->count(1); }),
            CCDelayTime::create(BEAT),
            CallFuncExt::create([this] { this->finish(); }),
            nullptr
        ));
    }

    void CardPicker::fly() {
        this->runAction(CCFadeTo::create(0.3f, DIM_COUNTDOWN));
        if (m_burst) {
            m_burst->runAction(CCSequence::create(CCFadeOut::create(0.3f), CCRemoveSelf::create(true), nullptr));
            m_burst = nullptr;
        }
        for (auto* node : { m_title, m_subtitle }) {
            node->runAction(CCSequence::create(CCDelayTime::create(0.7f), CCFadeOut::create(0.3f), nullptr));
        }
        if (!m_flying) {
            if (m_hooks.landed) {
                m_hooks.landed();
            }
            return;
        }

        auto const size = CCDirector::sharedDirector()->getWinSize();
        CCPoint target = m_hooks.target ? m_hooks.target() : CCPoint{ 30.f, size.height - 30.f };
        target = m_mainLayer->convertToNodeSpace(target);
        m_flying->runAction(CCSequence::create(
            CCSpawn::create(
                CCEaseIn::create(CCMoveTo::create(FLIGHT, target), 2.2f),
                CCScaleTo::create(FLIGHT, 0.16f),
                nullptr
            ),
            CallFuncExt::create([this] {
                if (m_hooks.landed) {
                    m_hooks.landed();
                }
            }),
            CCRemoveSelf::create(true),
            nullptr
        ));
        m_flying = nullptr;
    }

    void CardPicker::count(int value) {
        m_count->stopAllActions();
        auto const shown = value > 0 ? std::to_string(value) : tr("picker.go");
        text::set(m_count, shown, Font::Big, 1.7f);
        float const scale = m_count->getScale();
        m_count->setColor(value > 0 ? ccColor3B{ 255, 255, 255 } : ccColor3B{ 120, 255, 140 });
        m_count->setOpacity(255);
        m_count->setScale(scale * 1.55f);
        m_count->runAction(CCEaseBackOut::create(CCScaleTo::create(0.25f, scale)));
        art::play(value > 0 ? "sfx-tick.ogg" : "sfx-go.ogg", value > 0 ? 0.6f : 0.75f);
    }

    void CardPicker::finish() {
        if (m_finished) {
            return;
        }
        m_finished = true;
        Ref<CardPicker> self = this;

        this->count(0);
        // GO 와 함께 게임이 돈다. 이 화면은 터치와 키를 곧바로 놓아 준다. 남은 GO! 글자가
        // 사라지는 동안에도 바로 점프할 수 있어야 한다.
        this->setTouchEnabled(false);
        this->setKeypadEnabled(false);
        this->setKeyboardEnabled(false);
        if (m_hooks.resume) {
            m_hooks.resume();
        }

        m_count->runAction(CCSpawn::create(
            CCScaleTo::create(0.45f, m_count->getScale() * 1.35f),
            CCFadeOut::create(0.45f),
            nullptr
        ));
        this->runAction(CCSequence::create(
            CCFadeTo::create(0.2f, 0),
            CCDelayTime::create(0.25f),
            CCRemoveSelf::create(true),
            nullptr
        ));
    }

    void CardPicker::dismiss() {
        Ref<CardPicker> self = this;
        m_finished = true;
        this->stopAllActions();
        this->setTouchEnabled(false);
        this->setKeypadEnabled(false);
        this->setKeyboardEnabled(false);
        this->removeFromParent();
    }

    void CardPicker::keyBackClicked() {
        // 고르기 전에는 닫거나 건너뛸 수 없다.
    }

    void CardPicker::onClose(CCObject*) {
        // 위와 같다. 닫기 단추도 숨겨 두었다.
    }

    void CardPicker::keyDown(enumKeyCodes key, double) {
        // FLAlertLayer 의 keyDown 은 부르지 않는다. Enter 로 이 화면을 닫아 버린다.
        if (!m_ready || m_picked) {
            return;
        }
        int const last = static_cast<int>(m_faces.size()) - 1;
        switch (key) {
            case KEY_One: case KEY_NumPad1: this->pick(0); break;
            case KEY_Two: case KEY_NumPad2: this->pick(1); break;
            case KEY_Three: case KEY_NumPad3: this->pick(2); break;
            case KEY_Left: case CONTROLLER_Left:
                this->focus(m_focus < 0 ? last / 2 : m_focus - 1);
                break;
            case KEY_Right: case CONTROLLER_Right:
                this->focus(m_focus < 0 ? last / 2 : m_focus + 1);
                break;
            case KEY_Enter: case CONTROLLER_A:
                if (m_focus >= 0) {
                    this->pick(static_cast<std::size_t>(m_focus));
                }
                break;
            default: break;
        }
    }
}
