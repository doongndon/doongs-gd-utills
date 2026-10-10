#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>

#include <map>
#include <optional>
#include <string>
#include <vector>

// 오늘의 도전. 서버가 날마다 난이도별로 레벨 하나씩을 고르고, 모드를 쓰는 사람끼리
// 그 레벨을 가장 적은 시도로 깬 순서를 겨룬다. 서버는 daily-server/ 에 있다.
// 메인 레벨 칸 말고는 GD 서버의 레이팅된 레벨 가운데서 무작위로 고른다.
//
// 시도 수는 GD 가 레벨마다 세는 시도 수(m_attempts)에서 오늘 처음 그 레벨을 열 때의
// 값을 뺀 것이다. 연습 모드로 한 시도도 들어간다. 연습으로 몸을 풀어도 되지만 그만큼
// 시도가 쌓인다는 규칙이다. 연습 모드나 시작 위치로 깬 것은 깬 것으로 치지 않는다.
namespace gdu::daily {
    struct Row {
        std::string name;
        int attempts = 0;
    };

    struct Level {
        int id = 0;
        bool mainLevel = true;
        std::string name;
    };

    struct Tier {
        std::string id;
        std::string name;
        // 서버가 아직 고르지 못했으면(GD 서버가 안 받는 등) 비어 있다.
        std::optional<Level> level;
    };

    struct Today {
        std::string date;
        std::vector<Tier> tiers;
        // 아래 순위표가 어느 난이도의 것인지.
        std::string boardTier;
        std::vector<Row> top;
        int total = 0;
        std::optional<int> myRank;
    };

    // 이 기기에서 오늘 난이도 하나에 한 도전. 게임을 껐다 켜도 이어지도록 모드
    // 저장값에 둔다.
    struct Progress {
        int levelID = 0;
        // 오늘 처음 레벨을 열 때의 GD 시도 수. 아직 안 열었으면 -1.
        int baseline = -1;
        // 처음 깼을 때의 시도 수. 아직 못 깼으면 0.
        int clearedWith = 0;
        // 깬 기록을 서버가 받았는지. 못 보냈으면 다음에 오늘의 도전을 열 때 다시 보낸다.
        bool submitted = false;
    };

    bool configured();

    // 팝업에서 마지막으로 고른 난이도. 다음에 열 때도 그 난이도로 연다.
    std::string selectedTier();
    void selectTier(std::string const& tier);

    std::string todayUrl(std::string const& tier);

    // 서버의 답을 읽어 오늘의 도전으로 담는다. 실패하면 사람이 읽을 까닭을 돌려준다.
    std::optional<std::string> acceptToday(geode::utils::web::WebResponse const& response);

    // 메뉴가 처음 뜰 때 한 번 불러 둔다. 그래야 메뉴에서 바로 레벨에 들어가도
    // 오늘의 도전인지 알아본다.
    void refreshInBackground();

    std::optional<Today> const& today();
    Tier const* tier(std::string const& id);
    Progress progress(std::string const& tier);

    // 이 레벨이 오늘의 도전이면 그 난이도를 돌려준다.
    std::optional<std::string> tierOf(GJGameLevel* level);
    void startPlaying(GJGameLevel* level);
    void cleared(GJGameLevel* level);

    // 깼는데 서버가 못 받은 기록이 있으면 다시 보낸다.
    void resubmitPending();

    // 그 난이도의 레벨이 있는 곳으로 간다. 메인 레벨은 그 레벨의 쪽, 온라인 레벨은 검색 결과.
    void open(std::string const& tier);
}
