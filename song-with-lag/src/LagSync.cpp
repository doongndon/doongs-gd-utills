#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
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
// 노래도 멈춘 셈이 된다. 반대로 기기 전체가 멈춰 소리 출력까지 밀린 탓에
// 노래가 뒤처지면 그만큼 앞으로 당긴다. 어느 쪽이든 노래가 레벨을 따라간다.
//
// 렉인지는 게임이 넘겨주는 프레임 길이로 판단하면 안 된다. Smooth Fix 가
// 이미 바꿔 놓은 값이라 렉이 걸려도 평범해 보인다. 그래서 시계를 직접 읽는다.
namespace {
    // 레벨의 노래는 음악 번호 0 에서 돈다. 게임이 "음악이 나오고 있나" 를 물을
    // 때도 이 번호를 쓴다(GameManager 의 일시정지 처리).
    constexpr int kLevelMusic = 0;

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

        // 화면 정보 줄에 띄우는 숫자들. 레벨을 새로 들어올 때 0 으로 돌린다.
        int lagFrames = 0;
        int moves = 0;
        int lastMove = 0;
        double drift = 0.0;
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
    // 같은 양만큼 옮긴다. 그 곡들도 같은 시간만큼 어긋났기 때문이다.
    // shift 가 양수면 되감고, 음수면 앞으로 당긴다.
    void moveMusic(FMOD::Channel* main, unsigned int target, int shift) {
        main->setPosition(target, FMOD_TIMEUNIT_MS);

        auto* engine = FMODAudioEngine::get();
        for (auto& [musicID, music] : engine->m_fmodMusic) {
            auto* channel = engine->channelForChannelID(music.m_channelID);
            if (!channel || channel == main) continue;
            auto position = playingPositionMs(channel);
            if (!position) continue;
            long long const moved = static_cast<long long>(*position) - shift;
            // 되감을 만큼 흐르지 않은 채널은 방금 새로 시작한 것이다.
            if (moved < 0) continue;
            // 곡 길이를 넘기면 FMOD 가 거절하므로 따로 막지 않는다.
            channel->setPosition(static_cast<unsigned int>(moved), FMOD_TIMEUNIT_MS);
        }
    }

    // 화면 아래 가운데에 떠 있는 한 줄. 오른쪽 위는 일시정지 단추 자리라 피한다. 모드가 돌고 있는지, 노래를 찾았는지, 얼마나
    // 어긋났는지를 보여 준다. 이 줄이 안 보이면 모드가 아예 돌지 않는 것이다.
    void showInfo(CCNode* layer, std::string const& text) {
        auto* label = static_cast<CCLabelBMFont*>(layer->getChildByID("sync-info"_spr));
        if (!label) {
            label = CCLabelBMFont::create("", "bigFont.fnt");
            label->setID("sync-info"_spr);
            label->setScale(0.3f);
            label->setAnchorPoint({ 0.5f, 0.f });
            auto const winSize = CCDirector::get()->getWinSize();
            label->setPosition({ winSize.width / 2.f, 6.f });
            layer->addChild(label, 1000);
        }
        label->setVisible(true);
        label->setString(text.c_str());
    }

    void hideInfo(CCNode* layer) {
        if (auto* label = layer->getChildByID("sync-info"_spr)) {
            label->setVisible(false);
        }
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
            s.lagFrames++;
            // 긴 프레임은 평범한 한 프레임으로 줄인다. 레벨이 앞으로 순간이동하지
            // 않고 멈춘다. 그동안 앞서 간 노래는 followLevel 이 되감는다.
            dt = std::min(dt, s.normalDt);
        }
        else {
            s.normalDt = s.normalDt * 0.9f + wall * 0.1f;
        }

        GJBaseGameLayer::update(dt);
        s.levelMs += static_cast<double>(dt) * 1000.0;

        char const* status = this->followLevel(play, wall);

        if (config.showInfo) {
            showInfo(play, fmt::format(
                "song {}  diff {:+.0f}ms  lag {}  moved {} ({:+d}ms)",
                status, s.drift, s.lagFrames, s.moves, s.lastMove
            ));
        }
        else {
            hideInfo(play);
        }
    }

    // 노래를 레벨이 받은 시간에 맞춘다. 정보 줄에 띄울 상태 한 마디를 돌려준다.
    char const* followLevel(PlayLayer* play, float wall) {
        auto& s = state();
        auto& config = swl::Config::get();

        // 죽었거나 끝났거나 멈춘 동안은 노래가 따로 논다. 다시 움직이면
        // 그때 새로 기준을 잡는다.
        if (play->m_isPaused || play->m_hasCompletedLevel || m_playerDied) {
            dropAnchor();
            return "idle";
        }

        auto* channel = levelMusicChannel();
        auto position = playingPositionMs(channel);
        if (!position) {
            dropAnchor();
            return channel ? "stopped" : "not found";
        }

        unsigned int const music = *position;
        auto reanchor = [&] {
            s.anchored = true;
            s.refLevelMs = s.levelMs;
            s.refMusicMs = music;
            s.lastMusicMs = music;
            s.drift = 0.0;
        };

        if (!s.anchored) {
            reanchor();
            return "found";
        }

        // 이번 프레임에 노래가 시계보다 많이 움직였거나 뒤로 갔다면 누가
        // 위치를 옮긴 것이다. 렉이 아니니 그 자리에서 새로 기준을 잡는다.
        double const step = static_cast<double>(music) - static_cast<double>(s.lastMusicMs);
        if (step < -kSlackMs || step > wall * 1000.0 + kSlackMs) {
            reanchor();
            return "jumped";
        }
        s.lastMusicMs = music;

        double const expected = s.refMusicMs + (s.levelMs - s.refLevelMs);
        s.drift = static_cast<double>(music) - expected;
        if (std::abs(s.drift) <= config.tolerance || expected < 0.0) return "ok";

        auto const target = static_cast<unsigned int>(expected);
        int const shift = static_cast<int>(static_cast<long long>(music) - target);
        moveMusic(channel, target, shift);
        s.lastMusicMs = target;
        s.moves++;
        s.lastMove = -shift;

        log::debug("song moved by {} ms (frame took {} ms)", -shift, static_cast<int>(wall * 1000.f));
        return "moved";
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
        auto& s = state();
        dropAnchor();
        s.lastFrame.reset();
        s.lagFrames = 0;
        s.moves = 0;
        s.lastMove = 0;
        s.drift = 0.0;
        PlayLayer::onQuit();
    }
};
