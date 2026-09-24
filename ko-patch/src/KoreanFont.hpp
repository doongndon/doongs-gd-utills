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
}
