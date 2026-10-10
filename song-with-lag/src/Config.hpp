#pragma once

namespace swl {
    // mod.json 설정값을 미리 꺼내 둔 것. 게임 루프는 매 프레임 이 값을 보는데,
    // Mod::getSettingValue 는 문자열로 찾아 JSON 을 푸는 일이라 프레임마다
    // 부르기엔 아깝다. 값이 바뀔 때만 main.cpp 가 다시 읽어 넣는다.
    struct Config {
        bool enabled = true;
        // 이보다 긴 프레임을 렉으로 본다. 초 단위.
        float threshold = 0.04f;
        bool showNotice = false;

        static Config& get();

        void reload();
    };
}
