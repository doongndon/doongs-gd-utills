#include <Geode/Geode.hpp>
#include <Geode/modify/AppDelegate.hpp>

#include "Settings.hpp"

using namespace geode::prelude;

// 앱이 백그라운드로 내려갈 때 세이브를 강제한다.
//
// 모바일 OS 는 백그라운드로 간 앱을 예고 없이 죽인다. 그때까지 저장되지 않은
// 시도 횟수와 최고 기록은 그대로 사라진다. 나갈 때 문을 잠그듯, 내려가는
// 시점에 한 번 확실히 써 두면 다시 켰을 때 기록이 남아 있다.
class $modify(SaveGuardAppDelegate, AppDelegate) {
    void applicationDidEnterBackground() {
        AppDelegate::applicationDidEnterBackground();

        if (gdu::Settings::get().saveOnBackground) {
            this->trySaveGame(true);
        }
    }
};
