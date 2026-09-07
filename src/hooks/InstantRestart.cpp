#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "Settings.hpp"

using namespace geode::prelude;

// 죽자마자 바로 재시작한다.
//
// destroyPlayer 안에서 곧장 리셋하면 충돌 처리 도중에 레벨을 갈아엎는 셈이라
// 위험하다. 그래서 "재시작 예약"만 걸어 두고, 그 프레임의 물리 계산이 모두 끝난
// postUpdate 에서 실제로 리셋한다.
class $modify(InstantRestartPlayLayer, PlayLayer) {
    struct Fields {
        bool m_restartQueued = false;
    };

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        PlayLayer::destroyPlayer(player, object);

        if (!gdu::Settings::get().instantRestart) {
            return;
        }
        // 안티치트용 가짜 가시는 실제 죽음이 아니다.
        if (object == m_anticheatSpike) {
            return;
        }
        if (m_hasCompletedLevel || !player || !player->m_isDead) {
            return;
        }

        m_fields->m_restartQueued = true;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        if (!m_fields->m_restartQueued) {
            return;
        }
        m_fields->m_restartQueued = false;

        this->resetLevel();
    }
};
