#include "RunHud.hpp"

#include <Geode/ui/NineSlice.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>

#include <fmt/format.h>

#include "../Art.hpp"
#include "../Daily.hpp"

using namespace geode::prelude;

namespace {
    constexpr float HEIGHT = 46.f;
    constexpr float SLOT = 22.f;
    constexpr float SLOT_ICON = 16.f;
    constexpr float SLOT_Y = 13.f;
    constexpr float SLOT_X = 15.f;
    constexpr float SLOT_STEP = 26.f;
    constexpr float TOP_Y = 34.f;
    constexpr ccColor3B EMPTY{ 115, 115, 128 };

    float widthFor(std::size_t slots) {
        return std::max(116.f, SLOT_X + SLOT_STEP * static_cast<float>(slots) + 30.f);
    }

    CCPoint slotPosition(std::size_t index) {
        return { SLOT_X + SLOT_STEP * static_cast<float>(index), SLOT_Y };
    }

    std::string upper(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        return text;
    }

    std::string speedText(float speed) {
        if (std::abs(speed - 1.f) < 0.005f) {
            return "";
        }
        auto text = fmt::format("{:.2f}", speed);
        while (!text.empty() && text.back() == '0') {
            text.pop_back();
        }
        if (!text.empty() && text.back() == '.') {
            text.pop_back();
        }
        return text + "x";
    }
}

namespace gdu::ui {
    RunHud* RunHud::create(std::string const& tier, std::vector<int> const& milestones) {
        auto* hud = new RunHud();
        if (hud->init(tier, milestones)) {
            hud->autorelease();
            return hud;
        }
        delete hud;
        return nullptr;
    }

    bool RunHud::init(std::string const& tier, std::vector<int> const& milestones) {
        if (!CCNode::init()) {
            return false;
        }
        float const width = widthFor(milestones.size());
        this->setID("run-hud"_spr);
        this->setContentSize({ width, HEIGHT });
        this->setAnchorPoint({ 0.f, 1.f });

        auto* panel = NineSlice::create("square02b_001.png");
        panel->setContentSize({ width, HEIGHT });
        panel->setAnchorPoint({ 0.f, 0.f });
        panel->setColor({ 0, 0, 0 });
        panel->setOpacity(125);
        this->addChild(panel);
        m_panel = panel;

        if (auto* face = art::difficultyFace(daily::tierFace(tier))) {
            art::fit(face, 15.f);
            face->setPosition({ 12.f, TOP_Y });
            this->addChild(face, 1);
        }

        auto const* entry = daily::tier(tier);
        auto* name = CCLabelBMFont::create(upper(entry ? entry->name : tier).c_str(), "goldFont.fnt");
        name->setAnchorPoint({ 0.f, 0.5f });
        name->setPosition({ 22.f, TOP_Y + 0.5f });
        name->limitLabelWidth(width - 62.f, 0.42f, 0.2f);
        this->addChild(name, 1);

        m_score = CCLabelBMFont::create("0", "bigFont.fnt");
        m_score->setAnchorPoint({ 1.f, 0.5f });
        m_score->setScale(0.5f);
        m_score->setPosition({ width - 6.f, TOP_Y });
        this->addChild(m_score, 1);

        for (std::size_t i = 0; i < milestones.size(); ++i) {
            Slot slot;
            slot.frame = art::sprite("hud-slot.png");
            if (!slot.frame) {
                slot.frame = CCSprite::create("GJ_square02.png");
            }
            if (slot.frame) {
                art::fit(slot.frame, SLOT);
                slot.frame->setPosition(slotPosition(i));
                slot.frame->setColor(EMPTY);
                this->addChild(slot.frame, 1);
            }
            slot.empty = CCLabelBMFont::create(fmt::format("{}%", milestones[i]).c_str(), "chatFont.fnt");
            slot.empty->setScale(0.36f);
            slot.empty->setOpacity(170);
            slot.empty->setPosition(slotPosition(i));
            this->addChild(slot.empty, 2);
            m_slots.push_back(slot);
        }

        m_speed = CCLabelBMFont::create("", "bigFont.fnt");
        m_speed->setAnchorPoint({ 1.f, 0.5f });
        m_speed->setScale(0.32f);
        m_speed->setPosition({ width - 6.f, SLOT_Y });
        this->addChild(m_speed, 1);

        this->scheduleUpdate();
        return true;
    }

    void RunHud::refresh(
        std::vector<std::string> const& picks, int score, int usedShields, float speed, bool animate
    ) {
        int shieldsSeen = 0;
        for (std::size_t i = 0; i < m_slots.size(); ++i) {
            auto& slot = m_slots[i];
            std::string const id = i < picks.size() ? picks[i] : "";
            bool const fresh = slot.card != id;
            if (fresh) {
                if (slot.icon) {
                    slot.icon->removeFromParent();
                    slot.icon = nullptr;
                }
                slot.card = id;
                if (!id.empty()) {
                    if (auto* icon = art::cardIcon(id)) {
                        art::fit(icon, SLOT_ICON);
                        icon->setPosition(slotPosition(i));
                        this->addChild(icon, 3);
                        slot.icon = icon;
                        if (animate) {
                            float const scale = icon->getScale();
                            icon->setScale(scale * 2.4f);
                            icon->runAction(CCEaseBackOut::create(CCScaleTo::create(0.3f, scale)));
                        }
                    }
                }
            }

            auto const* card = daily::card(id);
            bool const help = card && card->help;
            if (slot.frame) {
                slot.frame->setColor(id.empty() ? EMPTY : art::kindColor(help));
                if (fresh && animate && !id.empty()) {
                    float const scale = slot.frame->getScale();
                    slot.frame->stopAllActions();
                    slot.frame->setScale(scale * 1.35f);
                    slot.frame->runAction(CCEaseBackOut::create(CCScaleTo::create(0.3f, scale)));
                }
            }
            slot.empty->setVisible(id.empty());

            // 이미 깨진 방패는 회색으로.
            if (id == "shield" && slot.icon) {
                bool const broken = shieldsSeen < usedShields;
                ++shieldsSeen;
                if (auto* sprite = typeinfo_cast<CCSprite*>(slot.icon)) {
                    sprite->setColor(broken ? ccColor3B{ 90, 90, 90 } : ccColor3B{ 255, 255, 255 });
                    sprite->setOpacity(broken ? 150 : 255);
                }
            }
        }

        if (m_target != score && animate) {
            m_score->stopAllActions();
            m_score->setScale(0.62f);
            m_score->runAction(CCEaseBackOut::create(CCScaleTo::create(0.3f, 0.5f)));
        }
        m_target = score;
        if (!animate) {
            m_shown = static_cast<float>(score);
            m_score->setString(std::to_string(score).c_str());
        }

        m_speed->setString(speedText(speed).c_str());
        m_speed->setColor(speed < 1.f ? art::kindLight(true) : ccColor3B{ 255, 170, 90 });
    }

    void RunHud::update(float dt) {
        float const target = static_cast<float>(m_target);
        if (std::abs(m_shown - target) < 0.5f) {
            if (m_shown == target) {
                return;
            }
            m_shown = target;
        }
        else {
            m_shown += (target - m_shown) * std::min(1.f, dt * 7.f);
        }
        m_score->setString(std::to_string(static_cast<int>(std::lround(m_shown))).c_str());
    }

    void RunHud::lost() {
        m_panel->stopAllActions();
        if (auto* panel = typeinfo_cast<NineSlice*>(m_panel)) {
            panel->setColor({ 210, 30, 60 });
            panel->runAction(CCTintTo::create(0.6f, 0, 0, 0));
        }
        for (auto& slot : m_slots) {
            if (slot.icon) {
                slot.icon->stopAllActions();
                slot.icon->runAction(CCSequence::create(
                    CCScaleTo::create(0.2f, 0.f),
                    CCRemoveSelf::create(true),
                    nullptr
                ));
                slot.icon = nullptr;
            }
            slot.card.clear();
            if (slot.frame) {
                slot.frame->setColor(EMPTY);
            }
            slot.empty->setVisible(true);
        }
    }

    void RunHud::shieldBroke() {
        this->stopActionByTag(11);
        auto* shake = CCSequence::create(
            CCMoveBy::create(0.04f, { 3.f, 0.f }),
            CCMoveBy::create(0.08f, { -6.f, 0.f }),
            CCMoveBy::create(0.08f, { 6.f, 0.f }),
            CCMoveBy::create(0.04f, { -3.f, 0.f }),
            nullptr
        );
        shake->setTag(11);
        this->runAction(shake);
    }

    void RunHud::showClear(int score, std::vector<std::string> const& picks) {
        auto* parent = this->getParent();
        if (!parent) {
            return;
        }
        auto const size = CCDirector::sharedDirector()->getWinSize();
        auto* banner = CCNode::create();
        banner->setID("run-clear"_spr);
        banner->setPosition({ size.width / 2.f, size.height - 80.f });
        parent->addChild(banner, this->getZOrder());

        auto* title = CCLabelBMFont::create("DAILY RUN CLEAR", "goldFont.fnt");
        title->setScale(0.75f);
        title->setPosition({ 0.f, 22.f });
        banner->addChild(title);

        auto* points = CCLabelBMFont::create(fmt::format("{} POINTS", score).c_str(), "bigFont.fnt");
        points->setScale(0.7f);
        points->setColor({ 255, 225, 100 });
        points->setPosition({ 0.f, 0.f });
        banner->addChild(points);

        float const step = 24.f;
        float const first = -step * static_cast<float>(picks.size() - (picks.empty() ? 0 : 1)) / 2.f;
        for (std::size_t i = 0; i < picks.size(); ++i) {
            if (auto* icon = art::cardIcon(picks[i])) {
                art::fit(icon, 20.f);
                icon->setPosition({ first + step * static_cast<float>(i), -24.f });
                banner->addChild(icon);
            }
        }

        banner->setScale(0.3f);
        banner->runAction(CCSequence::create(
            CCEaseBackOut::create(CCScaleTo::create(0.35f, 1.f)),
            CCDelayTime::create(2.8f),
            CCScaleTo::create(0.2f, 0.f),
            CCRemoveSelf::create(true),
            nullptr
        ));
    }

    CCPoint RunHud::slotWorld(std::size_t index) {
        return this->convertToWorldSpace(slotPosition(index));
    }
}
