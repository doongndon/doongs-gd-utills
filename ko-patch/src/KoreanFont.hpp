#pragma once

#include <string>
#include <string_view>

namespace kopatch {
    // 고른 글꼴과 금색 여부의 네 갈래. 두 훅이 같은 답을 봐야 하므로 한 곳에 둔다.
    std::string const& ownFont(bool pixel, bool gold);

    // GD 는 제목과 강조에 금색 글꼴을 쓴다. 번역했다고 전부 흰 글꼴로 바꿔
    // 버리면 제목과 본문이 같아 보여서, 화면의 위아래가 구분되지 않는다.
    bool wantsGold(std::string_view fontFile);

    // MultilineBitmapFont 는 문장을 조각내어 라벨 여러 개에 나눠 담는다. 그
    // 조각 하나하나가 CCLabelBMFont::setString 을 지나가기 때문에, 조각을
    // 번역하면 "Normal" 의 앞 두 글자 "No" 가 표에 걸려 "아니오rmal" 같은
    // 것이 나온다. 쪼개는 동안에는 라벨 훅을 재우고, 문장은 쪼개지기 전에
    // 통째로 바꾼다.
    bool splittingText();
    void setSplittingText(bool on);

    // 레벨 이름은 이름이다. "Jumper" 라는 레벨을 "점퍼" 로, 남이 "Test" 라고
    // 지은 레벨을 "시험" 으로 바꾸면 안 된다. 그런데 글자만 봐서는 그것이
    // 단추 글인지 누가 지은 이름인지 알 수 없다.
    //
    // 그래서 게임이 레벨 하나를 그리는 동안(레벨 칸, 레벨 정보, 일시정지 창..)
    // 그 레벨의 이름을 여기 올려 두고, 그 이름과 똑같은 글은 번역하지 않는다.
    // 그리는 동안만 막으므로, "Play" 라는 레벨이 있다고 다른 화면의 "Play"
    // 단추까지 영어로 남지는 않는다.
    class LevelNameScope {
    public:
        explicit LevelNameScope(std::string_view name);
        ~LevelNameScope();
        LevelNameScope(LevelNameScope const&) = delete;
        LevelNameScope& operator=(LevelNameScope const&) = delete;
    };

    bool isLevelNameInScope(std::string_view text);
}
