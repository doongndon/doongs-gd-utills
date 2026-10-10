#pragma once

#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// 오늘의 도전. 서버가 날마다 난이도별로 레벨 하나씩을 고르고, 모드를 쓰는 사람끼리
// 그 레벨을 로그라이크로 깨며 점수를 겨룬다. 서버는 daily-server/ 에 있다.
// 메인 레벨 칸 말고는 GD 서버의 레이팅된 레벨 가운데서 무작위로 고른다.
//
// 시도 수는 GD 가 레벨마다 세는 시도 수(m_attempts)에서 오늘 처음 그 레벨을 열 때의
// 값을 뺀 것이다. 연습 모드로 한 시도도 들어간다. 연습 모드나 시작 위치로 깬 것은
// 깬 것으로 치지 않는다.
namespace gdu::daily {
    struct Row {
        std::string name;
        int attempts = 0;
        std::optional<int> score;
        // 그 사람이 고른 카드. 예전 서버는 보내지 않는다.
        std::vector<std::string> cards;
    };

    // 카드 한 장. 효과는 Roguelike.cpp 가 id 를 보고 건다.
    struct Card {
        std::string name;
        std::string text;
        bool help = false;
        int points = 0;
    };

    // 그날의 로그라이크 규칙. 서버가 정하고 모두에게 같다. 칸마다 나오는 카드는
    // Tier::offers 에 있다.
    struct Roguelike {
        std::vector<int> milestones;
        int base = 100;
        std::map<std::string, Card> cards;
    };

    struct Level {
        int id = 0;
        bool mainLevel = true;
        std::string name;
        // 아래는 온라인 레벨에만 서버가 채운다. 메인 레벨은 게임에서 읽는다(lookOf).
        std::string creator;
        int stars = 0;
        // GD 의 난이도 얼굴 번호(Art.hpp 의 difficultyFace). 모르면 -1.
        int face = -1;
        // 0 없음, 1 피처드, 2 에픽, 3 레전더리, 4 미식
        int feature = 0;
        // 0 Tiny ~ 4 XL. 모르면 -1.
        int length = -1;
    };

    struct Tier {
        std::string id;
        std::string name;
        // 서버가 아직 고르지 못했으면(GD 서버가 안 받는 등) 비어 있다.
        std::optional<Level> level;
        // 지점(25, 50, 75%)마다 고를 수 있는 카드 세 장. 그날 그 칸을 하는 사람은 모두 같다.
        std::vector<std::vector<std::string>> offers;
    };

    struct Today {
        std::string date;
        std::vector<Tier> tiers;
        // 아래 순위표가 어느 난이도의 것인지.
        std::string boardTier;
        std::vector<Row> top;
        int total = 0;
        std::optional<int> myRank;
        std::optional<int> myScore;
        Roguelike roguelike;
    };

    // 이 기기에서 오늘 난이도 하나에 한 도전. 게임을 껐다 켜도 이어지도록 모드
    // 저장값에 둔다.
    struct Progress {
        int levelID = 0;
        // 오늘 처음 레벨을 열 때의 GD 시도 수. 아직 안 열었으면 -1.
        int baseline = -1;
        // 가장 좋은 판을 깼을 때의 시도 수. 아직 못 깼으면 0.
        int clearedWith = 0;
        // 깬 기록을 서버가 받았는지. 못 보냈으면 다음에 오늘의 도전을 열 때 다시 보낸다.
        bool submitted = false;
        // 가장 좋은 판에서 고른 카드와 그 점수.
        std::vector<std::string> cards;
        int score = 0;
        // 오늘 일반 모드로 가장 멀리 간 %.
        int bestPercent = 0;
    };

    // 레벨이 어떻게 보이는지. 메인 레벨은 GD 에서, 온라인 레벨은 서버가 준 값에서 읽는다.
    struct Look {
        int face = 0;
        int feature = 0;
        int stars = 0;
        std::string creator;
        // GD 가 쓰는 길이 이름(Tiny, Short ...). 모르면 비어 있다.
        std::string length;
    };

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
    // 다음 도전까지 남은 초. 오늘의 도전을 아직 모르면 비어 있다.
    std::optional<int> secondsLeft();
    Tier const* tier(std::string const& id);
    Progress progress(std::string const& tier);
    // 오늘 어느 난이도든 하나라도 깼는지. 메뉴 단추의 알림 점에 쓴다.
    bool clearedAnyToday();
    Card const* card(std::string const& id);

    // 난이도 칸의 기본 얼굴 번호. 레벨을 아직 모를 때 탭과 HUD 에 쓴다.
    int tierFace(std::string_view tier);
    Look lookOf(Tier const& tier);

    // 이 레벨이 오늘의 도전이면 그 난이도를 돌려준다.
    std::optional<std::string> tierOf(GJGameLevel* level);
    void startPlaying(GJGameLevel* level);
    // 일반 모드로 하다 죽었을 때. 팝업의 "오늘 최고 n%" 에 쓴다.
    void notePercent(GJGameLevel* level, int percent);
    // 오늘 그 난이도의 레벨에 쓴 시도 수. 레벨을 오늘 아직 안 열었거나 GD 가 그 레벨을
    // 들고 있지 않으면 비어 있다. 순위표의 시도 수와 같은 셈이다.
    std::optional<int> attemptsToday(std::string const& tier);
    // cards 는 이번 판에 고른 카드.
    void cleared(GJGameLevel* level, std::vector<std::string> const& cards);

    // 그 칸에서 이 카드들로 깼을 때의 점수. 그날 그 칸의 카드와 맞지 않으면 비어 있다.
    std::optional<int> scoreOf(std::string const& tier, std::vector<std::string> const& cards);
    // 고른 카드까지의 점수(판 도중). 기본 점수에 고른 카드의 점수를 더한다.
    int partialScore(std::vector<std::string> const& cards);

    // 깼는데 서버가 못 받은 기록이 있으면 다시 보낸다.
    void resubmitPending();

    // 그 난이도의 레벨이 있는 곳으로 간다. 메인 레벨은 그 레벨의 쪽, 온라인 레벨은
    // 받아 둔 레벨이면 그 레벨 화면, 아니면 검색 결과.
    void open(std::string const& tier);
}
