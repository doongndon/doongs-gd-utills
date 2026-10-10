#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <algorithm>
#include <random>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "Settings.hpp"
#include "daily/Daily.hpp"

using namespace geode::prelude;

// 로그라이크 데일리. 오늘의 도전 가운데 로그라이크 칸의 레벨을 일반 모드로 하면,
// 25%, 50%, 75% 에서 게임이 멈추고 카드 세 장이 나온다. 한 장을 고르면 3초 뒤
// 이어서 달린다. 죽으면 그 판의 카드는 모두 사라지고 처음부터 다시 고른다.
//
// 카드의 효과는 여기서 id 를 보고 건다. 이름과 설명, 점수, 그날 나오는 카드는
// 서버가 정한다(daily-server/server.js 의 CARDS).
namespace {
    constexpr int PICKER_Z = 1000;
    constexpr int OVERLAY_Z = 990;
    constexpr int HUD_Z = 995;
    constexpr float SAFE_SECONDS = 1.f;

    struct Run {
        // 지금 레벨이 로그라이크 칸의 레벨인지. 레벨을 여는 순간 정한다.
        bool level = false;
        std::size_t nextStop = 0;
        std::vector<std::string> picks;
        bool frozen = false;

        int shields = 0;
        float safeFor = 0.f;
        float speed = 1.f;
        bool slowed = false;
        bool rushed = false;
        bool mirrored = false;
        bool mini = false;
        bool shaking = false;
        bool invisible = false;
        bool muted = false;

        Ref<CCNode> dark;
        Ref<CCNode> blackout;
        Ref<CCNode> picker;
        Ref<CCLabelBMFont> hud;
    };

    Run g_run;

    FMOD::ChannelGroup* musicChannel() {
        auto* engine = FMODAudioEngine::sharedEngine();
        return engine ? engine->m_backgroundMusicChannel : nullptr;
    }

    gdu::daily::Card const* cardOf(std::string const& id) {
        auto const& today = gdu::daily::today();
        if (!today) {
            return nullptr;
        }
        auto const found = today->roguelike.cards.find(id);
        return found == today->roguelike.cards.end() ? nullptr : &found->second;
    }

    void updateHud() {
        if (!g_run.hud) {
            return;
        }
        auto const score = gdu::daily::scoreOf(g_run.picks);
        std::string text = "Roguelike";
        int points = 0;
        if (auto const& today = gdu::daily::today()) {
            points = today->roguelike.base;
        }
        std::vector<std::string> names;
        for (auto const& id : g_run.picks) {
            if (auto const* card = cardOf(id)) {
                points += card->points;
                names.push_back(card->name);
            }
        }
        text += fmt::format("  {} pts", score.value_or(points));
        if (g_run.shields > 0) {
            text += fmt::format("  Shield x{}", g_run.shields);
        }
        if (!names.empty()) {
            text += "\n";
            for (std::size_t i = 0; i < names.size(); ++i) {
                text += (i ? ", " : "") + names[i];
            }
        }
        g_run.hud->setString(text.c_str());
    }

    // 카드 효과를 모두 걷어 낸다. 판이 새로 시작되거나, 레벨을 나가거나, 연습 모드로
    // 바뀔 때 부른다. 화면 반전과 작은 아이콘은 우리가 건 것일 때만 되돌린다.
    void clearEffects(PlayLayer* layer) {
        if (g_run.frozen && layer) {
            layer->resumeAudio();
        }
        g_run.frozen = false;

        if (g_run.picker) {
            g_run.picker->removeFromParent();
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
            if (g_run.mirrored) {
                layer->toggleFlipped(false, true);
            }
            for (auto* player : { layer->m_player1, layer->m_player2 }) {
                if (!player) {
                    continue;
                }
                if (g_run.mini) {
                    player->togglePlayerScale(false, true);
                }
                player->setVisible(true);
            }
            layer->setPosition({ 0.f, 0.f });
        }

        bool const level = g_run.level;
        auto hud = g_run.hud;
        g_run = Run{};
        g_run.level = level;
        g_run.hud = hud;
        updateHud();
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
            g_run.dark = addOverlay(layer, 170);
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

    // 카드를 고르는 화면. 게임은 그동안 멈춰 있다(아래 GJBaseGameLayer::update 훅).
    // 고르면 3, 2, 1 을 센 뒤 스스로 사라지며 게임을 다시 돌린다.
    class CardPicker : public CCLayerColor {
    public:
        static CardPicker* create(std::size_t stop, std::vector<std::string> const& offer) {
            auto* picker = new CardPicker();
            if (picker->init(stop, offer)) {
                picker->autorelease();
                return picker;
            }
            delete picker;
            return nullptr;
        }

    private:
        CCMenu* m_menu = nullptr;
        CCLabelBMFont* m_title = nullptr;
        CCLabelBMFont* m_count = nullptr;
        std::vector<std::string> m_offer;
        int m_left = 3;

        bool init(std::size_t stop, std::vector<std::string> const& offer) {
            auto const size = CCDirector::sharedDirector()->getWinSize();
            if (!CCLayerColor::initWithColor({ 0, 0, 0, 160 }, size.width, size.height)) {
                return false;
            }
            this->setID("roguelike-picker"_spr);
            m_offer = offer;

            int percent = 0;
            if (auto const& today = gdu::daily::today(); today && stop < today->roguelike.milestones.size()) {
                percent = today->roguelike.milestones[stop];
            }
            m_title = CCLabelBMFont::create(
                fmt::format("{}% - pick a card", percent).c_str(), "bigFont.fnt");
            m_title->setScale(0.6f);
            m_title->setPosition({ size.width / 2.f, size.height / 2.f + 105.f });
            this->addChild(m_title);

            m_menu = CCMenu::create();
            m_menu->setPosition({ 0.f, 0.f });
            this->addChild(m_menu);

            constexpr float CARD_W = 120.f;
            constexpr float CARD_H = 150.f;
            constexpr float GAP = 14.f;
            float const total = CARD_W * static_cast<float>(offer.size()) + GAP * static_cast<float>(offer.size() - 1);
            for (std::size_t i = 0; i < offer.size(); ++i) {
                auto* face = this->makeCard(offer[i], CARD_W, CARD_H);
                auto* button = CCMenuItemSpriteExtra::create(face, this, menu_selector(CardPicker::onPick));
                button->setTag(static_cast<int>(i));
                button->setPosition({
                    size.width / 2.f - total / 2.f + CARD_W / 2.f + static_cast<float>(i) * (CARD_W + GAP),
                    size.height / 2.f - 5.f
                });
                m_menu->addChild(button);
            }

            m_count = CCLabelBMFont::create("", "bigFont.fnt");
            m_count->setPosition({ size.width / 2.f, size.height / 2.f });
            m_count->setVisible(false);
            this->addChild(m_count);
            return true;
        }

        CCNode* makeCard(std::string const& id, float width, float height) {
            auto const* card = cardOf(id);
            bool const help = card && card->help;

            auto* face = CCNode::create();
            face->setContentSize({ width, height });
            face->setAnchorPoint({ 0.5f, 0.5f });

            auto* background = CCLayerColor::create(
                help ? ccColor4B{ 22, 78, 46, 240 } : ccColor4B{ 78, 24, 66, 240 }, width, height);
            face->addChild(background);

            auto* kind = CCLabelBMFont::create(help ? "HELP" : "CURSE", "goldFont.fnt");
            kind->setScale(0.45f);
            kind->setPosition({ width / 2.f, height - 14.f });
            face->addChild(kind);

            auto* name = CCLabelBMFont::create(card ? card->name.c_str() : id.c_str(), "bigFont.fnt");
            name->limitLabelWidth(width - 14.f, 0.55f, 0.2f);
            name->setPosition({ width / 2.f, height - 40.f });
            face->addChild(name);

            auto* text = CCLabelBMFont::create(
                card ? card->text.c_str() : "", "chatFont.fnt", (width - 16.f) / 0.6f, kCCTextAlignmentCenter);
            text->setScale(0.6f);
            text->setPosition({ width / 2.f, height / 2.f - 8.f });
            face->addChild(text);

            int const points = card ? card->points : 0;
            auto* score = CCLabelBMFont::create(
                fmt::format("{}{} pts", points > 0 ? "+" : "", points).c_str(), "goldFont.fnt");
            score->setScale(0.5f);
            score->setColor(points >= 0 ? ccColor3B{ 120, 255, 120 } : ccColor3B{ 255, 140, 140 });
            score->setPosition({ width / 2.f, 16.f });
            face->addChild(score);
            return face;
        }

        void onPick(CCObject* sender) {
            auto const index = static_cast<std::size_t>(static_cast<CCNode*>(sender)->getTag());
            auto* layer = PlayLayer::get();
            if (!layer || index >= m_offer.size() || !m_menu->isEnabled()) {
                return;
            }
            // 단추 줄을 이 콜백 안에서 지우지 않는다. 숨기고 막아만 둔다.
            m_menu->setEnabled(false);
            m_menu->setVisible(false);

            g_run.picks.push_back(m_offer[index]);
            applyCard(layer, m_offer[index]);
            updateHud();

            auto const* card = cardOf(m_offer[index]);
            m_title->setString(fmt::format("{} - get ready", card ? card->name : m_offer[index]).c_str());
            m_count->setVisible(true);
            this->step();
            this->runAction(CCSequence::create(
                CCDelayTime::create(1.f),
                CCCallFunc::create(this, callfunc_selector(CardPicker::step)),
                CCDelayTime::create(1.f),
                CCCallFunc::create(this, callfunc_selector(CardPicker::step)),
                CCDelayTime::create(1.f),
                CCCallFunc::create(this, callfunc_selector(CardPicker::resume)),
                nullptr
            ));
        }

        void step() {
            m_count->setString(std::to_string(m_left).c_str());
            m_count->setScale(1.4f);
            m_count->runAction(CCEaseBackOut::create(CCScaleTo::create(0.3f, 1.f)));
            --m_left;
        }

        // 다시 달린다. 이 화면은 g_run 이 붙들고 있으므로 거기서 놓아 지운다.
        void resume() {
            auto* layer = PlayLayer::get();
            if (g_run.frozen && layer) {
                layer->resumeAudio();
            }
            g_run.frozen = false;
            if (g_run.picker.data() == this) {
                this->removeFromParent();
                g_run.picker = nullptr;
            }
        }
    };

    bool runsHere(PlayLayer* layer) {
        return g_run.level && layer && !layer->m_isPracticeMode && !layer->m_isTestMode;
    }
}

class $modify(RoguelikePlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        clearEffects(nullptr);
        g_run.level = false;
        g_run.hud = nullptr;

        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        auto const tier = gdu::daily::tierOf(level);
        g_run.level = gdu::Settings::get().dailyEnabled && tier && *tier == gdu::daily::ROGUELIKE;
        if (!g_run.level) {
            return true;
        }

        auto const size = CCDirector::sharedDirector()->getWinSize();
        auto* hud = CCLabelBMFont::create("", "chatFont.fnt");
        hud->setID("roguelike-hud"_spr);
        hud->setScale(0.55f);
        hud->setAnchorPoint({ 0.5f, 1.f });
        hud->setPosition({ size.width / 2.f, size.height - 6.f });
        hud->setAlignment(kCCTextAlignmentCenter);
        this->addChild(hud, HUD_Z);
        g_run.hud = hud;
        updateHud();
        return true;
    }

    // 죽으면 다시 시작하며 여기를 지난다. 로그라이크이므로 카드도 모두 잃는다.
    void resetLevel() {
        PlayLayer::resetLevel();
        if (g_run.level) {
            clearEffects(this);
        }
    }

    void onQuit() {
        if (g_run.level || g_run.frozen) {
            clearEffects(this);
        }
        g_run = Run{};
        PlayLayer::onQuit();
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
                g_run.safeFor = SAFE_SECONDS;
                updateHud();
                Notification::create("Shield!", NotificationIcon::Info, 1.f)->show();
                return;
            }
        }
        PlayLayer::destroyPlayer(player, object);
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (!g_run.level) {
            return;
        }
        // 연습 모드로 바꾸면 그 판은 로그라이크가 아니다. 효과를 걷는다.
        if (m_isPracticeMode || m_isTestMode) {
            if (!g_run.picks.empty() || g_run.frozen) {
                clearEffects(this);
            }
            return;
        }

        if (g_run.safeFor > 0.f) {
            g_run.safeFor = std::max(0.f, g_run.safeFor - dt);
        }
        if (g_run.invisible) {
            for (auto* player : { m_player1, m_player2 }) {
                if (player) {
                    player->setVisible(false);
                }
            }
        }
        if (g_run.shaking) {
            static std::mt19937 random(std::random_device{}());
            std::uniform_real_distribution<float> jitter(-3.f, 3.f);
            this->setPosition({ jitter(random), jitter(random) });
        }

        auto const& today = gdu::daily::today();
        if (!today || g_run.frozen || m_hasCompletedLevel) {
            return;
        }
        auto const& rules = today->roguelike;
        if (g_run.nextStop >= rules.milestones.size() || g_run.nextStop >= rules.offers.size()) {
            return;
        }
        if (this->getCurrentPercent() < static_cast<float>(rules.milestones[g_run.nextStop])) {
            return;
        }

        auto* picker = CardPicker::create(g_run.nextStop, rules.offers[g_run.nextStop]);
        ++g_run.nextStop;
        if (!picker) {
            return;
        }
        g_run.frozen = true;
        this->pauseAudio();
        this->addChild(picker, PICKER_Z);
        g_run.picker = picker;
    }

    void levelComplete() {
        auto const picks = g_run.picks;
        bool const counts = runsHere(this);
        PlayLayer::levelComplete();
        if (counts && gdu::Settings::get().dailyEnabled) {
            gdu::daily::cleared(m_level, picks);
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
