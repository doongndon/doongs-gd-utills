#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <fmt/format.h>

#include "ClickCounter.hpp"
#include "Settings.hpp"

using namespace geode::prelude;

namespace {
    // 초당 20번만 갱신한다. CCLabelBMFont::setString 은 글자 스프라이트를 다시
    // 만들기 때문에, 눈에 보이지도 않는 240fps 갱신은 낭비다.
    constexpr float REFRESH_INTERVAL = 1.f / 20.f;
    constexpr float EDGE_PADDING = 5.f;
    // 왼쪽 위에는 일시정지 버튼이 있으므로 그만큼 내려서 겹치지 않게 한다.
    constexpr float PAUSE_BUTTON_CLEARANCE = 34.f;

    void appendTime(std::string& out, double seconds) {
        if (seconds < 0.0) {
            seconds = 0.0;
        }
        auto const total = static_cast<int>(seconds * 100.0);
        fmt::format_to(
            std::back_inserter(out), "{}:{:02}.{:02}",
            total / 6000, total / 100 % 60, total % 100
        );
    }
}

// 클릭 집계는 GJBaseGameLayer 단계에서 받는다. PlayLayer 뿐 아니라 연습/테스트
// 모드에서도 같은 경로로 입력이 들어오기 때문이다.
class $modify(ClickCounterBGL, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        GJBaseGameLayer::handleButton(down, button, isPlayer1);

        if (down && gdu::Settings::get().showCps) {
            gdu::ClickCounter::get().push(gdu::ClickCounter::now());
        }
    }
};

class $modify(InfoHudPlayLayer, PlayLayer) {
    struct Fields {
        CCLabelBMFont* m_hud = nullptr;
        std::string m_text;
        float m_sinceRefresh = REFRESH_INTERVAL;
        unsigned int m_appliedRevision = 0;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        gdu::ClickCounter::get().reset();

        auto* parent = m_uiLayer ? static_cast<CCNode*>(m_uiLayer) : static_cast<CCNode*>(this);
        auto* hud = CCLabelBMFont::create("", "chatFont.fnt");
        hud->setID("info-hud"_spr);
        hud->setOpacity(200);
        parent->addChild(hud, 100);

        m_fields->m_hud = hud;
        m_fields->m_text.reserve(64);
        this->applyHudLayout();

        return true;
    }

    void applyHudLayout() {
        auto const& settings = gdu::Settings::get();
        auto* hud = m_fields->m_hud;
        if (!hud) {
            return;
        }

        m_fields->m_appliedRevision = settings.revision;
        hud->setVisible(settings.hudEnabled);
        hud->setScale(settings.hudScale);

        auto const size = CCDirector::sharedDirector()->getWinSize();
        auto const& position = settings.hudPosition;
        bool const isRight = position == "top-right" || position == "bottom-right";
        bool const isTop = position == "top-left" || position == "top-right";

        float const top = position == "top-left"
            ? size.height - PAUSE_BUTTON_CLEARANCE
            : size.height - EDGE_PADDING;

        hud->setAnchorPoint({ isRight ? 1.f : 0.f, isTop ? 1.f : 0.f });
        hud->setPosition({
            isRight ? size.width - EDGE_PADDING : EDGE_PADDING,
            isTop ? top : EDGE_PADDING
        });
    }

    void refreshHud() {
        auto const& settings = gdu::Settings::get();
        auto& text = m_fields->m_text;
        text.clear();

        if (settings.showAttempts && m_level) {
            fmt::format_to(std::back_inserter(text), "Attempt {}\n", m_level->m_attempts.value());
        }

        if (settings.showBest && m_level) {
            if (m_isPracticeMode) {
                fmt::format_to(std::back_inserter(text), "Best {}% (P)\n", m_level->m_practicePercent);
            }
            else {
                fmt::format_to(std::back_inserter(text), "Best {}%\n", m_level->m_normalPercent.value());
            }
        }

        if (settings.showPercent) {
            fmt::format_to(std::back_inserter(text), "{:.1f}%\n", this->getCurrentPercent());
        }

        if (settings.showTimer) {
            appendTime(text, m_attemptTime);
            text.push_back('\n');
        }

        if (settings.showCps) {
            fmt::format_to(
                std::back_inserter(text), "{} CPS ({})\n",
                gdu::ClickCounter::get().perSecond(gdu::ClickCounter::now()),
                gdu::ClickCounter::get().total()
            );
        }

        if (!text.empty()) {
            text.pop_back();
        }

        auto* hud = m_fields->m_hud;
        if (text.empty()) {
            hud->setVisible(false);
            return;
        }

        hud->setVisible(true);
        hud->setString(text.c_str());
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        auto* hud = m_fields->m_hud;
        if (!hud) {
            return;
        }

        auto const& settings = gdu::Settings::get();
        if (m_fields->m_appliedRevision != settings.revision) {
            this->applyHudLayout();
        }

        if (!settings.hudEnabled) {
            return;
        }

        m_fields->m_sinceRefresh += dt;
        if (m_fields->m_sinceRefresh < REFRESH_INTERVAL) {
            return;
        }
        m_fields->m_sinceRefresh = 0.f;

        this->refreshHud();
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        gdu::ClickCounter::get().reset();
    }
};
