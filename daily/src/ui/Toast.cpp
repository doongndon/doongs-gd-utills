#include "Toast.hpp"

#include <Geode/Geode.hpp>
#include <Geode/ui/NineSlice.hpp>
#include <Geode/ui/OverlayManager.hpp>

#include <algorithm>

#include "../Art.hpp"
#include "../Text.hpp"

using namespace geode::prelude;

namespace {
    // 떠 있는 알림. 다음 알림이 오면 이것을 내린다. 힙에 두고 놓지 않는다(정적 소멸자가
    // cocos 보다 늦게 돌지 않게).
    WeakRef<CCNode>& current() {
        static auto* ref = new WeakRef<CCNode>(nullptr);
        return *ref;
    }
}

namespace gdu::ui {
    void toast(std::string const& text, bool good) {
        auto* overlay = OverlayManager::get();
        if (!overlay) {
            return;
        }
        if (auto old = current().lock()) {
            old->stopAllActions();
            old->removeFromParent();
        }

        auto const size = CCDirector::sharedDirector()->getWinSize();
        auto* node = CCNode::create();
        node->setID("toast"_spr);

        auto* label = text::label(text, text::Font::Big, 0.5f);
        text::fit(label, text::Font::Big, size.width * 0.75f, 0.5f, 0.2f);
        auto const labelSize = label->getScaledContentSize();

        CCSprite* icon = CCSprite::createWithSpriteFrameName(good ? "GJ_completesIcon_001.png" : "GJ_deleteIcon_001.png");
        float const iconSize = 16.f;
        float const gap = icon ? 6.f : 0.f;
        float const width = labelSize.width + (icon ? iconSize + gap : 0.f) + 16.f;
        float const height = std::max(labelSize.height, iconSize) + 12.f;

        auto* bg = NineSlice::create("square02b_small.png", { 0, 0, 40, 40 });
        bg->setContentSize({ width, height });
        bg->setColor({ 0, 0, 0 });
        bg->setOpacity(170);
        node->addChild(bg);

        float x = -width / 2.f + 8.f;
        if (icon) {
            art::fit(icon, iconSize);
            icon->setPosition({ x + iconSize / 2.f, 0.f });
            node->addChild(icon);
            x += iconSize + gap;
        }
        label->setAnchorPoint({ 0.f, 0.5f });
        label->setPosition({ x, 0.f });
        node->addChild(label);

        node->setPosition({ size.width / 2.f, size.height / 4.f - 30.f });
        node->setScale(0.85f);
        overlay->addChild(node, 1000);
        current() = node;

        float const time = good ? 4.f : 5.f;
        node->runAction(CCSequence::create(
            CCSpawn::create(
                CCEaseExponentialOut::create(CCMoveBy::create(0.35f, { 0.f, 30.f })),
                CCEaseExponentialOut::create(CCScaleTo::create(0.35f, 1.f)),
                nullptr
            ),
            CCDelayTime::create(time),
            CCEaseBackIn::create(CCScaleTo::create(0.25f, 0.f)),
            CCRemoveSelf::create(true),
            nullptr
        ));
    }
}
