#pragma once

#include <Geode/Geode.hpp>

#include <string>

namespace gdu::session {
    // 마지막으로 들어간 온라인 레벨 기록.
    //
    // active 는 "레벨 안에 있는 채로 게임이 끝났다"는 뜻이다. 정상적으로 나가면
    // 꺼지므로, 다시 켰을 때 이 값이 켜져 있다면 앱이 도중에 죽었다는 신호다.
    struct Record {
        int levelID = 0;
        std::string levelName;
        bool active = false;
    };

    // 레벨에 들어갈 때 호출. 온라인 레벨이 아니면 아무 것도 하지 않는다.
    void begin(GJGameLevel* level);

    // 레벨에서 정상적으로 나갈 때 호출.
    void end();

    Record current();
}
