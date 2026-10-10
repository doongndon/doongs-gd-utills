#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <algorithm>
#include <chrono>
#include <optional>

#include "Config.hpp"

using namespace geode::prelude;

// 렉이 걸리면 노래도 함께 렉이 걸리게 한다.
//
// 렉이 걸린 프레임에서 게임은 흘러간 시간을 다 진행하지 않는다. 게임의
// "Smooth Fix" 는 튀는 프레임 길이를 평범한 1/60 초로 바꿔치기하고, 이 모드도
// 긴 프레임을 평범한 한 프레임으로 줄인다. 그런데 노래는 FMOD 의 제 스레드에서
// 멈추지 않고 흘러가므로, 렉 한 번마다 노래가 그만큼 레벨보다 앞서 간다.
//
// 그래서 레벨이 실제로 받은 시간과 노래가 실제로 흐른 시간을 따로 센다.
// 노래가 레벨보다 앞서 가면 그 차이만큼 노래를 되감는다. 레벨이 멈춘 만큼
// 노래도 멈춘 셈이 된다.
//
// 렉인지는 게임이 넘겨주는 프레임 길이로 판단하면 안 된다. Smooth Fix 가
// 이미 바꿔 놓은 값이라 렉이 걸려도 평범해 보인다. 그래서 시계를 직접 읽는다.
namespace {
    // 레벨의 노래는 음악 번호 0 에서 돈다. 게임이 "음악이 나오고 있나" 를 물을
    // 때도 이 번호를 쓴다(GameManager 의 일시정지 처리).
    constexpr int kLevelMusic = 0;

    // FMOD 가 알려 주는 위치는 믹스 블록 단위로 움직여 수십 ms 쯤 흔들린다.
    // 이보다 작은 어긋남은 되감지 않는다. 되감을 때마다 소리가 살짝 튀기 때문이다.
    constexpr double kToleranceMs = 50.0;

    // 한 프레임 동안 노래가 시계보다 이만큼 더 움직였다면 렉이 아니라 누가
    // 노래 위치를 옮긴 것이다(재시작, 체크포인트, 반복 재생).
    constexpr double kSlackMs = 80.0;

    using Clock = std::chrono::steady_clock;

    struct State {
        // 레벨이 지금까지 받은 시간의 합(ms). 게임이 실제로 진행한 시간이다.
        double levelMs = 0.0;

        // 기준점: 이때 레벨 시간이 refLevelMs 였고 노래 위치가 refMusicMs 였다.
        // 그 뒤로 노래는 레벨이 받은 만큼만 흘러야 한다.
        bool anchored = false;
        double refLevelMs = 0.0;
        double refMusicMs = 0.0;

        // 직전 프레임 끝의 노래 위치와 시각. 위치가 갑자기 옮겨졌는지 본다.
        unsigned int lastMusicMs = 0;
        std::optional<Clock::time_point> lastFrame;

        // 렉이 아닌 프레임의 평균 길이(초). 렉 프레임 대신 이만큼 진행한다.
        float normalDt = 1.f / 60.f;
    };

    State& state() {
        static State s;
        return s;
    }

    // 소리가 실제로 흘러가고 있는 채널의 위치만 돌려준다. 멈췄거나 일시정지된
    // 채널은 시간이 흐르지 않으니 맞출 것도 없다.
    std::optional<unsigned int> playingPositionMs(FMOD::Channel* channel) {
        if (!channel) return std::nullopt;

        bool playing = false;
        if (channel->isPlaying(&playing) != FMOD_OK || !playing) return std::nullopt;

        bool paused = true;
        if (channel->getPaused(&paused) != FMOD_OK || paused) return std::nullopt;

        unsigned int position = 0;
        if (channel->getPosition(&position, FMOD_TIMEUNIT_MS) != FMOD_OK) return std::nullopt;
        return position;
    }

    FMOD::Channel* levelMusicChannel() {
        auto* engine = FMODAudioEngine::get();
        if (!engine) return nullptr;
        if (auto* channel = engine->getActiveMusicChannel(kLevelMusic)) return channel;

        // 번호로 못 찾으면 배경 음악 묶음의 첫 채널을 본다.
        FMOD::Channel* channel = nullptr;
        if (engine->m_backgroundMusicChannel &&
            engine->m_backgroundMusicChannel->getChannel(0, &channel) == FMOD_OK) {
            return channel;
        }
        return nullptr;
    }

    // 레벨 노래는 정확히 target 으로 옮기고, 송 트리거로 함께 틀어 둔 곡은
    // 같은 양만큼 되감는다. 그 곡들도 같은 시간 동안 앞서 갔기 때문이다.
    void rewindMusic(FMOD::Channel* main, unsigned int target, unsigned int amount) {
        main->setPosition(target, FMOD_TIMEUNIT_MS);

        auto* engine = FMODAudioEngine::get();
        for (auto& [musicID, music] : engine->m_fmodMusic) {
            auto* channel = engine->channelForChannelID(music.m_channelID);
            if (!channel || channel == main) continue;
            auto position = playingPositionMs(channel);
            // 되감을 만큼 흐르지 않은 채널은 방금 새로 시작한 것이다.
            if (!position || *position < amount) continue;
            channel->setPosition(*position - amount, FMOD_TIMEUNIT_MS);
        }
    }

    void showNotice(CCNode* layer, std::string const& text) {
        auto* label = static_cast<CCLabelBMFont*>(layer->getChildByID("lag-notice"_spr));
        if (!label) {
            label = CCLabelBMFont::create("", "bigFont.fnt");
            label->setID("lag-notice"_spr);
            label->setScale(0.35f);
            label->setAnchorPoint({ 1.f, 1.f });
            auto const winSize = CCDirector::get()->getWinSize();
            label->setPosition({ winSize.width - 6.f, winSize.height - 6.f });
            layer->addChild(label, 1000);
        }

        label->setString(text.c_str());
        label->stopAllActions();
        label->setOpacity(255);
        label->runAction(CCSequence::create(
            CCDelayTime::create(0.8f),
            CCFadeOut::create(0.4f),
            nullptr
        ));
    }

    void dropAnchor() {
        state().anchored = false;
    }
}

class $modify(SongWithLagLayer, GJBaseGameLayer) {
    void update(float dt) {
        auto& config = swl::Config::get();
        auto* play = PlayLayer::get();

        // 편집기의 시험 플레이도 이 함수를 지나지만, 거기는 노래를 다루는
        // 방식이 달라 손대지 않는다.
        if (!config.enabled || !play || static_cast<GJBaseGameLayer*>(play) != this) {
            GJBaseGameLayer::update(dt);
            return;
        }

        auto& s = state();

        // 이번 프레임이 실제로 걸린 시간. 게임이 넘겨준 dt 는 Smooth Fix 가
        // 이미 고쳐 놓았을 수 있으니 믿지 않는다.
        auto const now = Clock::now();
        float wall = dt;
        if (s.lastFrame) {
            wall = std::chrono::duration<float>(now - *s.lastFrame).count();
        }
        s.lastFrame = now;

        bool const lagging = wall > config.threshold;
        if (lagging) {
            // 긴 프레임은 평범한 한 프레임으로 줄인다. 레벨이 앞으로 순간이동하지
            // 않고 멈춘다. 앞서 간 노래는 아래에서 되감는다.
            dt = std::min(dt, s.normalDt);
        }
        else {
            s.normalDt = s.normalDt * 0.9f + wall * 0.1f;
        }

        GJBaseGameLayer::update(dt);
        s.levelMs += static_cast<double>(dt) * 1000.0;

        this->followLevel(play, wall, lagging);
    }

    // 노래를 레벨이 받은 시간에 맞춘다.
    void followLevel(PlayLayer* play, float wall, bool lagging) {
        auto& s = state();
        auto& config = swl::Config::get();

        // 죽었거나 끝났거나 멈춘 동안은 노래가 따로 논다. 다시 움직이면
        // 그때 새로 기준을 잡는다.
        if (play->m_isPaused || play->m_hasCompletedLevel || m_playerDied) {
            dropAnchor();
            return;
        }

        auto* channel = levelMusicChannel();
        auto position = playingPositionMs(channel);
        if (!position) {
            if (lagging && config.showNotice) {
                showNotice(play, fmt::format("lag {} ms: no music", static_cast<int>(wall * 1000.f)));
            }
            dropAnchor();
            return;
        }

        unsigned int const music = *position;
        auto reanchor = [&] {
            s.anchored = true;
            s.refLevelMs = s.levelMs;
            s.refMusicMs = music;
            s.lastMusicMs = music;
        };

        if (!s.anchored) {
            reanchor();
            return;
        }

        // 이번 프레임에 노래가 시계보다 많이 움직였거나 뒤로 갔다면 누가
        // 위치를 옮긴 것이다. 렉이 아니니 그 자리에서 새로 기준을 잡는다.
        double const step = static_cast<double>(music) - static_cast<double>(s.lastMusicMs);
        if (step < -kSlackMs || step > wall * 1000.0 + kSlackMs) {
            reanchor();
            return;
        }
        s.lastMusicMs = music;

        double const expected = s.refMusicMs + (s.levelMs - s.refLevelMs);
        double const ahead = static_cast<double>(music) - expected;
        if (ahead <= kToleranceMs || expected < 0.0) return;

        auto const target = static_cast<unsigned int>(expected);
        auto const amount = music - target;
        rewindMusic(channel, target, amount);
        s.lastMusicMs = target;

        log::debug("song rewound by {} ms (frame took {} ms)", amount, static_cast<int>(wall * 1000.f));
        if (config.showNotice) {
            showNotice(play, fmt::format("-{} ms", amount));
        }
    }
};

// 일시정지 메뉴에서 다시 시작하거나 레벨을 나가면 노래 위치가 프레임 밖에서
// 바뀐다. 기준을 버려 다음 프레임에 다시 잡는다.
class $modify(SongWithLagPlayLayer, PlayLayer) {
    void resetLevel() {
        PlayLayer::resetLevel();
        dropAnchor();
    }

    void onQuit() {
        dropAnchor();
        state().lastFrame.reset();
        PlayLayer::onQuit();
    }
};
