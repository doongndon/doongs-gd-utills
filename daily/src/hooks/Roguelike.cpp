#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

#include "../Art.hpp"
#include "../Daily.hpp"
#include "../Text.hpp"
#include "../ui/CardPicker.hpp"
#include "../ui/RunHud.hpp"

using namespace geode::prelude;

// 오늘의 도전은 모두 로그라이크다. 어느 난이도든 그날의 레벨을 일반 모드로 하면,
// 25%, 50%, 75% 에서 게임이 멈추고 카드 세 장이 나온다. 한 장을 고르면 3, 2, 1 을
// 센 뒤 이어서 달린다. 죽으면 그 판의 카드는 모두 사라지고 처음부터 다시 고른다.
//
// 카드의 효과는 여기서 id 를 보고 건다. 이름과 설명, 점수, 그날 나오는 카드는
// 서버가 정한다(daily-server/server.js 의 CARDS).
namespace {
    constexpr int OVERLAY_Z = 990;
    constexpr int HUD_Z = 995;
    constexpr float SAFE_SECONDS = 1.f;

    struct Run {
        // 지금 레벨이 오늘의 도전 레벨인지, 그렇다면 어느 칸인지. 레벨을 여는 순간 정한다.
        bool level = false;
        std::string tier;
        std::size_t nextStop = 0;
        std::vector<std::string> picks;
        bool frozen = false;

        int shields = 0;
        int usedShields = 0;
        float safeFor = 0.f;
        float blink = 0.f;
        float speed = 1.f;
        bool slowed = false;
        bool rushed = false;
        bool mirrored = false;
        bool mini = false;
        bool shaking = false;
        bool invisible = false;
        // 유령 카드나 방패 깜빡임으로 우리가 아이콘을 숨겨 둔 상태인지.
        bool hidden = false;
        bool muted = false;

        Ref<CCNode> dark;
        Ref<CCNode> blackout;
        Ref<gdu::ui::CardPicker> picker;
        Ref<gdu::ui::RunHud> hud;
        std::vector<Ref<CCSprite>> ticks;
    };

    // 게임을 끌 때 정적 소멸자가 cocos 보다 늦게 돌며 노드를 지우지 않도록 힙에 두고 놓지 않는다.
    Run& g_run = *new Run();

    FMOD::ChannelGroup* musicChannel() {
        auto* engine = FMODAudioEngine::sharedEngine();
        return engine ? engine->m_backgroundMusicChannel : nullptr;
    }

    void updateHud(bool animate) {
        if (g_run.hud) {
            g_run.hud->refresh(
                g_run.picks, gdu::daily::partialScore(g_run.picks), g_run.usedShields, g_run.speed, animate
            );
        }
    }

    // 진행 막대 위의 표시. 카드를 고른 지점은 그 카드의 색이 된다.
    void paintTicks() {
        for (std::size_t i = 0; i < g_run.ticks.size(); ++i) {
            auto* tick = g_run.ticks[i].data();
            if (!tick) {
                continue;
            }
            if (i < g_run.picks.size()) {
                auto const* card = gdu::daily::card(g_run.picks[i]);
                tick->setColor(gdu::art::kindLight(card && card->help));
                tick->setOpacity(255);
            }
            else {
                tick->setColor({ 255, 255, 255 });
                tick->setOpacity(200);
            }
        }
    }

    // 아이콘만 숨긴다. PlayerObject 자체의 보이기는 GD 가 쓰므로(두 번째 플레이어는 듀얼
    // 모드에서만 보인다) 건드리지 않고, 아이콘을 담은 m_mainLayer 만 켜고 끈다.
    void hidePlayers(PlayLayer* layer, bool hide) {
        if (!layer || g_run.hidden == hide) {
            return;
        }
        g_run.hidden = hide;
        for (auto* player : { layer->m_player1, layer->m_player2 }) {
            if (player && player->m_mainLayer) {
                player->m_mainLayer->setVisible(!hide);
            }
        }
    }

    // 카드 효과를 모두 걷어 낸다. 판이 새로 시작되거나, 레벨을 나가거나, 연습 모드로
    // 바뀔 때 부른다. 화면 반전과 작은 아이콘은 우리가 건 것일 때만 되돌린다.
    // afterReset 이면 GD 가 방금 판을 처음 상태로 돌려놓았으므로(반전, 크기 포함) 그
    // 둘은 건드리지 않는다. 미니로 시작하는 레벨을 큰 아이콘으로 바꿔 버리지 않게.
    void clearEffects(PlayLayer* layer, bool afterReset = false) {
        if (g_run.frozen && layer) {
            layer->resumeAudio();
        }
        g_run.frozen = false;

        if (g_run.picker) {
            g_run.picker->dismiss();
            g_run.picker = nullptr;
        }
        for (auto* overlay : { &g_run.dark, &g_run.blackout }) {
            if (*overlay) {
                (*overlay)->removeFromParent();
                *overlay = nullptr;
            }
        }

        if (auto* channel = musicChannel()) {
            channel->setPitch(1.f);
            channel->setMute(false);
        }

        if (layer) {
            if (g_run.mirrored && !afterReset) {
                layer->toggleFlipped(false, true);
            }
            if (g_run.mini && !afterReset) {
                for (auto* player : { layer->m_player1, layer->m_player2 }) {
                    if (player) {
                        player->togglePlayerScale(false, true);
                    }
                }
            }
            hidePlayers(layer, false);
            layer->setPosition({ 0.f, 0.f });
        }

        bool const level = g_run.level;
        auto const tier = g_run.tier;
        auto hud = g_run.hud;
        auto ticks = g_run.ticks;
        g_run = Run{};
        g_run.level = level;
        g_run.tier = tier;
        g_run.hud = hud;
        g_run.ticks = ticks;
        paintTicks();
    }

    CCLayerColor* addOverlay(PlayLayer* layer, GLubyte opacity) {
        auto const size = CCDirector::sharedDirector()->getWinSize();
        auto* overlay = CCLayerColor::create({ 0, 0, 0, opacity }, size.width, size.height);
        overlay->setID("roguelike-overlay"_spr);
        layer->addChild(overlay, OVERLAY_Z);
        return overlay;
    }

    void applyCard(PlayLayer* layer, std::string const& id) {
        if (id == "shield") {
            ++g_run.shields;
        }
        else if (id == "slow" && !g_run.slowed) {
            g_run.slowed = true;
            g_run.speed *= 0.85f;
        }
        else if (id == "fast" && !g_run.rushed) {
            g_run.rushed = true;
            g_run.speed *= 1.2f;
        }
        else if (id == "mirror" && !g_run.mirrored) {
            g_run.mirrored = true;
            layer->toggleFlipped(true, false);
        }
        else if (id == "mini" && !g_run.mini) {
            g_run.mini = true;
            for (auto* player : { layer->m_player1, layer->m_player2 }) {
                if (player) {
                    player->togglePlayerScale(true, false);
                }
            }
        }
        else if (id == "dark" && !g_run.dark) {
            auto* overlay = addOverlay(layer, 0);
            overlay->runAction(CCFadeTo::create(0.6f, 170));
            g_run.dark = overlay;
        }
        else if (id == "shake") {
            g_run.shaking = true;
        }
        else if (id == "invisible") {
            g_run.invisible = true;
        }
        else if (id == "mute") {
            g_run.muted = true;
        }
        else if (id == "flash" && !g_run.blackout) {
            auto* overlay = addOverlay(layer, 0);
            overlay->runAction(CCRepeatForever::create(CCSequence::create(
                CCDelayTime::create(2.2f),
                CCFadeTo::create(0.08f, 255),
                CCDelayTime::create(0.35f),
                CCFadeTo::create(0.15f, 0),
                nullptr
            )));
            g_run.blackout = overlay;
        }

        if (auto* channel = musicChannel()) {
            channel->setPitch(g_run.speed);
            channel->setMute(g_run.muted);
        }
    }

    bool runsHere(PlayLayer* layer) {
        return g_run.level && layer && !layer->m_isPracticeMode && !layer->m_isTestMode;
    }

    // 방패가 죽음을 막았을 때. 화면이 하얗게 번쩍이고 SHIELD! 가 뜬다.
    void shieldFlash(PlayLayer* layer) {
        auto const size = CCDirector::sharedDirector()->getWinSize();
        auto* flash = CCLayerColor::create({ 200, 245, 255, 150 }, size.width, size.height);
        flash->runAction(CCSequence::create(CCFadeTo::create(0.3f, 0), CCRemoveSelf::create(true), nullptr));
        layer->addChild(flash, HUD_Z - 1);

        auto* text = gdu::text::label(std::string(gdu::text::tr("shield")), gdu::text::Font::Big, 1.f);
        float const scale = text->getScale();
        text->setColor({ 150, 235, 255 });
        text->setPosition({ size.width / 2.f, size.height * 0.68f });
        text->setScale(scale * 0.3f);
        text->runAction(CCSequence::create(
            CCEaseBackOut::create(CCScaleTo::create(0.2f, scale)),
            CCDelayTime::create(0.3f),
            CCSpawn::create(CCFadeOut::create(0.3f), CCMoveBy::create(0.3f, { 0.f, 12.f }), nullptr),
            CCRemoveSelf::create(true),
            nullptr
        ));
        layer->addChild(text, HUD_Z + 1);
        gdu::art::play("sfx-shield.ogg", 0.7f);
    }
}

class $modify(RoguelikePlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        clearEffects(nullptr);
        g_run = Run{};

        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        auto const tier = gdu::daily::tierOf(level);
        g_run.level = tier.has_value();
        g_run.tier = tier.value_or("");
        auto const& today = gdu::daily::today();
        if (!g_run.level || !today) {
            return true;
        }

        auto const size = CCDirector::sharedDirector()->getWinSize();
        if (auto* hud = gdu::ui::RunHud::create(g_run.tier, today->roguelike.milestones)) {
            hud->setPosition({ 8.f, size.height - 8.f });
            this->addChild(hud, HUD_Z);
            g_run.hud = hud;
        }

        // 진행 막대 위에 카드가 나오는 지점을 찍는다. 막대를 숨기면 같이 숨는다.
        if (m_progressBar) {
            auto const bar = m_progressBar->getContentSize();
            float const scale = m_progressBar->getScale();
            for (int percent : today->roguelike.milestones) {
                if (auto* tick = gdu::art::sprite("hud-tick.png")) {
                    gdu::art::fit(tick, scale > 0.f ? 9.f / scale : 9.f);
                    tick->setPosition({ 2.f + (bar.width - 4.f) * static_cast<float>(percent) / 100.f, bar.height / 2.f });
                    m_progressBar->addChild(tick, 5);
                    g_run.ticks.push_back(tick);
                }
            }
        }

        updateHud(false);
        paintTicks();
        return true;
    }

    // 죽거나 다시 시작하면 여기를 지난다. 로그라이크이므로 카드도 모두 잃는다.
    void resetLevel() {
        bool const lostCards = g_run.level && !g_run.picks.empty();
        PlayLayer::resetLevel();
        if (!g_run.level) {
            return;
        }
        clearEffects(this, true);
        if (lostCards && g_run.hud) {
            g_run.hud->lost();
            gdu::art::play("sfx-lose.ogg", 0.5f);
        }
        updateHud(false);
    }

    void onQuit() {
        if (g_run.level || g_run.frozen) {
            clearEffects(this);
        }
        g_run = Run{};
        PlayLayer::onQuit();
    }

    // 일시 정지를 풀면 GD 가 음악을 다시 튼다. 카드를 고르는 중이면 다시 멈춘다.
    void resume() {
        PlayLayer::resume();
        if (g_run.frozen) {
            this->pauseAudio();
        }
    }

    // 방패가 있으면 한 번 버티고, 그 뒤 잠깐은 무엇에 닿아도 죽지 않는다. 안티치트용
    // 가짜 가시는 진짜 죽음이 아니므로 그대로 둔다.
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (runsHere(this) && object != m_anticheatSpike && !m_hasCompletedLevel) {
            if (g_run.safeFor > 0.f) {
                return;
            }
            if (g_run.shields > 0) {
                --g_run.shields;
                ++g_run.usedShields;
                g_run.safeFor = SAFE_SECONDS;
                shieldFlash(this);
                if (g_run.hud) {
                    g_run.hud->shieldBroke();
                }
                updateHud(false);
                return;
            }
            gdu::daily::notePercent(m_level, this->getCurrentPercentInt());
        }
        PlayLayer::destroyPlayer(player, object);
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (!g_run.level) {
            return;
        }
        // 연습 모드나 시작 위치로 하는 판은 로그라이크가 아니다. 효과를 걷고 HUD 를 숨긴다.
        bool const counts = runsHere(this);
        if (g_run.hud) {
            g_run.hud->setVisible(counts);
        }
        for (auto& tick : g_run.ticks) {
            if (tick) {
                tick->setVisible(counts);
            }
        }
        if (!counts) {
            if (!g_run.picks.empty() || g_run.frozen) {
                clearEffects(this);
                updateHud(false);
            }
            return;
        }

        if (g_run.safeFor > 0.f) {
            g_run.safeFor = std::max(0.f, g_run.safeFor - dt);
            g_run.blink += dt;
        }
        // 방패가 깨진 뒤 무적인 동안은 아이콘이 깜빡인다.
        bool const blinkOff = g_run.safeFor > 0.f && std::fmod(g_run.blink, 0.12f) > 0.06f;
        hidePlayers(this, g_run.invisible || blinkOff);
        if (g_run.shaking) {
            static std::mt19937 random(std::random_device{}());
            std::uniform_real_distribution<float> jitter(-3.f, 3.f);
            this->setPosition({ jitter(random), jitter(random) });
        }

        auto const& today = gdu::daily::today();
        // 죽어서 다시 시작을 기다리는 동안에는 카드를 내지 않는다.
        bool const dead = m_player1 && m_player1->m_isDead;
        if (!today || g_run.frozen || m_hasCompletedLevel || dead) {
            return;
        }
        auto const& rules = today->roguelike;
        auto const* entry = gdu::daily::tier(g_run.tier);
        if (!entry || g_run.nextStop >= rules.milestones.size() || g_run.nextStop >= entry->offers.size()) {
            return;
        }
        if (this->getCurrentPercent() < static_cast<float>(rules.milestones[g_run.nextStop])) {
            return;
        }

        std::size_t const stop = g_run.nextStop++;
        gdu::ui::CardPicker::Hooks hooks;
        hooks.picked = [this](std::string const& id) {
            g_run.picks.push_back(id);
            applyCard(this, id);
        };
        hooks.target = [stop]() -> CCPoint {
            if (g_run.hud) {
                return g_run.hud->slotWorld(stop);
            }
            auto const size = CCDirector::sharedDirector()->getWinSize();
            return { 30.f, size.height - 30.f };
        };
        hooks.landed = [] {
            updateHud(true);
            paintTicks();
        };
        hooks.resume = [this] {
            if (g_run.frozen) {
                this->resumeAudio();
            }
            g_run.frozen = false;
            g_run.picker = nullptr;
            // 고르는 동안 뗀 점프 키는 카드 화면이 받아 버려 게임은 아직 누르고 있다고
            // 여길 수 있다. GO 에서 모든 누름을 풀어, 다시 눌러야 뛰게 한다.
            if (m_uiLayer) {
                m_uiLayer->resetAllButtons();
            }
            this->handleButton(false, 1, true);
            this->handleButton(false, 1, false);
        };

        auto* picker = gdu::ui::CardPicker::create(rules.milestones[stop], entry->offers[stop], std::move(hooks));
        if (!picker) {
            return;
        }
        g_run.frozen = true;
        this->pauseAudio();
        g_run.picker = picker;
        picker->show();
    }

    void levelComplete() {
        auto const picks = g_run.picks;
        bool const counts = runsHere(this);
        PlayLayer::levelComplete();
        if (!counts) {
            return;
        }
        gdu::daily::cleared(m_level, picks);
        if (g_run.hud) {
            int const score = gdu::daily::scoreOf(g_run.tier, picks).value_or(gdu::daily::partialScore(picks));
            g_run.hud->showClear(score, picks);
        }
    }
};

// 카드를 고르는 동안에는 게임이 흐르지 않는다. 슬로우와 가속은 게임에 넘기는 시간을
// 늘이고 줄여서 건다. 메뉴나 다른 화면의 시간은 건드리지 않는다.
class $modify(RoguelikeBaseLayer, GJBaseGameLayer) {
    void update(float dt) {
        if (g_run.level && static_cast<GJBaseGameLayer*>(PlayLayer::get()) == this) {
            if (g_run.frozen) {
                return;
            }
            dt *= g_run.speed;
        }
        GJBaseGameLayer::update(dt);
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        if (g_run.frozen && down) {
            return;
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }
};
