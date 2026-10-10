#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <algorithm>
#include <optional>

#include "Config.hpp"

using namespace geode::prelude;

// 렉이 걸리면 노래도 함께 렉이 걸리게 한다.
//
// 보통 게임은 한 프레임이 길어지면 그 시간만큼 레벨을 한꺼번에 진행한다.
// 노래는 FMOD 의 제 스레드에서 멈추지 않고 흘러가므로, 둘은 맞아 있지만
// 화면에서는 플레이어가 순간이동한 것처럼 보인다.
//
// 이 모드는 반대로 한다. 긴 프레임이 오면 레벨은 평소 한 프레임만큼만
// 나아가게 하고, 게임이 멈춰 있던 동안 노래가 앞서 간 만큼 노래를 되감는다.
// 레벨 시간과 노래 시간이 똑같이 "멈춘" 셈이라 싱크가 유지된다.
//
// 되감을 양은 짐작하지 않고 잰다. 직전 프레임이 끝날 때 노래 위치를 적어 두고,
// 이번 프레임에서 실제로 얼마나 흘렀는지 본다. 기기 전체가 멈춰 노래도 덜
// 흘렀다면 그만큼만 되감는다.
namespace {
    // 레벨의 노래는 음악 번호 0 에서 돈다. 게임이 "음악이 나오고 있나" 를 물을
    // 때도 이 번호를 쓴다(GameManager 의 일시정지 처리).
    constexpr int kLevelMusic = 0;

    // FMOD 가 알려 주는 위치는 믹스 블록 단위로 움직여 수십 ms 쯤 어긋날 수
    // 있다. 그 정도는 같은 흐름으로 본다.
    constexpr float kSlackMs = 80.f;

    struct State {
        // 직전 프레임이 끝났을 때 레벨 노래의 위치. 모르면 비워 둔다.
        std::optional<unsigned int> anchorMs;
        // 렉이 아닌 프레임의 평균 길이. 렉 프레임 대신 이만큼 진행한다.
        float normalDt = 1.f / 60.f;
    };

    State& state() {
        static State s;
        return s;
    }

    // 소리가 실제로 흘러가고 있는 채널의 위치만 돌려준다. 멈췄거나 일시정지된
    // 채널은 시간이 흐르지 않으니 되감을 것도 없다.
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

    std::optional<unsigned int> levelMusicMs() {
        auto* engine = FMODAudioEngine::get();
        if (!engine) return std::nullopt;
        return playingPositionMs(engine->getActiveMusicChannel(kLevelMusic));
    }

    // 지금 흐르는 모든 음악 채널을 같은 양만큼 되감는다. 송 트리거로 함께
    // 틀어 둔 곡이 있으면 그것도 같은 시간 동안 앞서 갔기 때문이다.
    void rewindAllMusic(unsigned int ms) {
        auto* engine = FMODAudioEngine::get();
        if (!engine) return;

        for (auto& [musicID, music] : engine->m_fmodMusic) {
            auto* channel = engine->channelForChannelID(music.m_channelID);
            auto position = playingPositionMs(channel);
            // 되감을 만큼 흐르지 않은 채널은 이번 렉 중에 새로 시작한 것이다.
            if (!position || *position < ms) continue;
            channel->setPosition(*position - ms, FMOD_TIMEUNIT_MS);
        }
    }

    void showNotice(CCNode* layer, unsigned int ms) {
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

        label->setString(fmt::format("-{} ms", ms).c_str());
        label->stopAllActions();
        label->setOpacity(255);
        label->runAction(CCSequence::create(
            CCDelayTime::create(0.6f),
            CCFadeOut::create(0.4f),
            nullptr
        ));
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

        if (dt <= config.threshold) {
            s.normalDt = s.normalDt * 0.9f + dt * 0.1f;
            GJBaseGameLayer::update(dt);
            s.anchorMs = levelMusicMs();
            return;
        }

        if (auto lost = this->absorbableLagMs(play, dt)) {
            // 노래를 먼저 되감는다. 이번 진행 중에 송 트리거가 새 곡을 틀면,
            // 그 곡은 되감지 않아야 하기 때문이다.
            rewindAllMusic(*lost);
            GJBaseGameLayer::update(std::min(s.normalDt, config.threshold));
            if (config.showNotice) showNotice(play, *lost);
        }
        else {
            GJBaseGameLayer::update(dt);
        }
        s.anchorMs = levelMusicMs();
    }

    // 이번 렉에서 노래가 앞서 간 ms. 되감을 수 없거나 되감으면 안 되는
    // 경우에는 비워서 돌려주고, 그때는 게임이 원래 하던 대로 둔다. 레벨만 멈추고
    // 노래를 못 되감으면 오히려 싱크가 어긋나기 때문이다.
    std::optional<unsigned int> absorbableLagMs(PlayLayer* play, float dt) {
        auto& s = state();
        if (!s.anchorMs) return std::nullopt;

        // 죽었거나 끝났거나 멈춘 동안은 노래가 따로 논다.
        if (play->m_isPaused || play->m_hasCompletedLevel || m_playerDied) return std::nullopt;

        auto now = levelMusicMs();
        if (!now || *now < *s.anchorMs) return std::nullopt;

        // 프레임 길이보다 노래가 더 흘렀다면 그사이에 누가 위치를 옮긴 것이다
        // (재시작, 체크포인트, 반복 재생). 그건 렉이 아니다.
        float const elapsed = static_cast<float>(*now - *s.anchorMs);
        if (elapsed > dt * 1000.f + kSlackMs) return std::nullopt;

        float const fed = std::min(s.normalDt, swl::Config::get().threshold) * 1000.f;
        if (elapsed <= fed) return std::nullopt;

        return static_cast<unsigned int>(elapsed - fed);
    }
};

// 일시정지 메뉴에서 다시 시작하거나 레벨을 나가면 노래 위치가 프레임 밖에서
// 바뀐다. 적어 둔 위치를 버려 다음 프레임에 다시 잰다.
class $modify(SongWithLagPlayLayer, PlayLayer) {
    void resetLevel() {
        PlayLayer::resetLevel();
        state().anchorMs.reset();
    }

    void onQuit() {
        state().anchorMs.reset();
        PlayLayer::onQuit();
    }
};
