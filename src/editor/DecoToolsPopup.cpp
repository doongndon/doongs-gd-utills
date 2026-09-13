#include "DecoToolsPopup.hpp"

#include <Geode/ui/BreakLine.hpp>

#include <cstdlib>
#include <fmt/format.h>

#include "DecoTools.hpp"

using namespace geode::prelude;

namespace {
    constexpr float WIDTH = 380.f;
    constexpr float HEIGHT = 300.f;
    constexpr float CENTER = WIDTH / 2.f;

    // 빈 칸은 "이 항목은 건드리지 마라"는 뜻이다.
    std::optional<float> parse(TextInput* input) {
        auto const text = std::string(input->getString());
        if (text.empty()) {
            return std::nullopt;
        }

        char* end = nullptr;
        float const value = std::strtof(text.c_str(), &end);
        if (end == text.c_str()) {
            return std::nullopt;
        }
        return value;
    }

    void report(std::size_t changed, char const* what) {
        if (changed == 0) {
            Notification::create(
                "Nothing to apply - select objects and fill in a value",
                NotificationIcon::Warning
            )->show();
            return;
        }
        Notification::create(
            fmt::format("{} applied to {} objects", what, changed), NotificationIcon::Success
        )->show();
    }
}

namespace gdu {
    DecoToolsPopup* DecoToolsPopup::create(EditorUI* editorUI) {
        auto* popup = new DecoToolsPopup();
        if (popup->init(editorUI)) {
            popup->autorelease();
            return popup;
        }
        delete popup;
        return nullptr;
    }

    bool DecoToolsPopup::init(EditorUI* editorUI) {
        if (!Popup::init(WIDTH, HEIGHT)) {
            return false;
        }

        m_editorUI = editorUI;
        this->setTitle("Deco Tools");

        this->addLabel(30.f, 248.f, "SCATTER - random jitter, blank = skip", 0.4f, "chatFont.fnt");
        this->addLabel(90.f, 228.f, "Position", 0.45f, "goldFont.fnt");
        this->addLabel(190.f, 228.f, "Rotation", 0.45f, "goldFont.fnt");
        this->addLabel(290.f, 228.f, "Scale %", 0.45f, "goldFont.fnt");
        m_scatterPosition = this->addInput(90.f, 203.f, 80.f, "0");
        m_scatterRotation = this->addInput(190.f, 203.f, 80.f, "0");
        m_scatterScale = this->addInput(290.f, 203.f, 80.f, "0");
        this->addButton(CENTER, 167.f, "Scatter", menu_selector(DecoToolsPopup::onApplyScatter));

        auto* line = BreakLine::create(WIDTH - 60.f);
        line->setPosition({ 30.f, 144.f });
        m_mainLayer->addChild(line);

        this->addLabel(30.f, 126.f, "RAMP - gradual change, left to right", 0.4f, "chatFont.fnt");
        this->addLabel(102.f, 106.f, "Rotation", 0.45f, "goldFont.fnt");
        this->addLabel(284.f, 106.f, "Scale", 0.45f, "goldFont.fnt");
        m_rampRotationStart = this->addInput(70.f, 82.f, 56.f, "from");
        this->addLabel(102.f, 82.f, "to", 0.4f, "chatFont.fnt");
        m_rampRotationEnd = this->addInput(134.f, 82.f, 56.f, "to");
        m_rampScaleStart = this->addInput(252.f, 82.f, 56.f, "from");
        this->addLabel(284.f, 82.f, "to", 0.4f, "chatFont.fnt");
        m_rampScaleEnd = this->addInput(316.f, 82.f, 56.f, "to");
        this->addButton(CENTER, 44.f, "Ramp", menu_selector(DecoToolsPopup::onApplyRamp));

        return true;
    }

    TextInput* DecoToolsPopup::addInput(float x, float y, float width, char const* placeholder) {
        auto* input = TextInput::create(width, placeholder);
        input->setCommonFilter(CommonFilter::Float);
        input->setMaxCharCount(7);
        input->setPosition({ x, y });
        m_mainLayer->addChild(input);
        return input;
    }

    void DecoToolsPopup::addLabel(float x, float y, char const* text, float scale, char const* font) {
        auto* label = CCLabelBMFont::create(text, font);
        label->setScale(scale);
        label->setPosition({ x, y });
        m_mainLayer->addChild(label);
    }

    void DecoToolsPopup::addButton(float x, float y, char const* text, SEL_MenuHandler handler) {
        auto* sprite = ButtonSprite::create(text, "goldFont.fnt", "GJ_button_01.png", 0.8f);
        auto* button = CCMenuItemSpriteExtra::create(sprite, this, handler);
        button->setScale(0.75f);
        button->setPosition({ x, y });
        m_buttonMenu->addChild(button);
    }

    void DecoToolsPopup::onApplyScatter(CCObject*) {
        report(deco::scatter(m_editorUI, deco::ScatterOptions{
            .position = parse(m_scatterPosition),
            .rotation = parse(m_scatterRotation),
            .scale = parse(m_scatterScale),
        }), "Scatter");
    }

    void DecoToolsPopup::onApplyRamp(CCObject*) {
        report(deco::ramp(m_editorUI, deco::RampOptions{
            .rotationStart = parse(m_rampRotationStart),
            .rotationEnd = parse(m_rampRotationEnd),
            .scaleStart = parse(m_rampScaleStart),
            .scaleEnd = parse(m_rampScaleEnd),
        }), "Ramp");
    }
}
