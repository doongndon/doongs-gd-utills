#pragma once

#include <Geode/Geode.hpp>

#include <string>

namespace gdu {
    // mod.json 설정값 캐시.
    //
    // 비유하자면, 매 프레임 냉장고 문을 여는 대신 필요한 재료를 도마 위에 미리
    // 꺼내 두는 것과 같다. Mod::getSettingValue 는 문자열 조회 + JSON 변환이라
    // 게임 루프 안에서 부르기엔 비싸므로, 값이 실제로 바뀔 때만 갱신한다.
    struct Settings {
        bool hudEnabled = true;
        std::string hudPosition = "top-left";
        float hudScale = 0.45f;
        bool showAttempts = true;
        bool showBest = true;
        bool showPercent = true;
        bool showTimer = true;
        bool showCps = true;

        bool instantRestart = false;
        bool pauseInfo = true;

        bool saveOnBackground = true;
        bool resumeButton = true;

        // 갱신될 때마다 증가한다. HUD 는 이 값만 비교해 다시 배치할지 정한다.
        unsigned int revision = 0;

        static Settings& get();

        void reload();
    };
}
