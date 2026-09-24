#pragma once

#include <Geode/Geode.hpp>

#include <functional>
#include <string>
#include <string_view>

// 번역표에 없는 문장을 Gemini 에게 물어본다.
//
// 완성된 번역기로 쓰자는 게 아니다. 후킹 지점이 CCLabelBMFont 하나라서, 거기
// 들어오는 글자가 게임 문구인지 남이 지은 레벨 이름인지 구분할 방법이 없다.
// 그래서 답을 파일에 쌓아 두고 사람이 보고 고르게 한다. 쓸 만한 것만 ko.json
// 으로 옮기면 그때부터는 모두에게 적용된다.
namespace kopatch::gemini {
    void configure(bool enabled, std::string key, std::string model);
    bool enabled();

    // 저장해 둔 답을 읽는다. 모드가 켜질 때 한 번.
    void load();

    // 이미 받아 둔 번역. 없으면 nullptr.
    std::string const* find(std::string_view source);

    // 아직 물어본 적 없는 문장만 물어본다. 답이 오면 주 스레드에서 콜백한다.
    void request(std::string source, std::function<void(std::string const&)> onTranslated);

    // 물어볼 가치가 있는 글자인지. 숫자와 기호뿐이면 물어볼 것도 없다.
    bool worthAsking(std::string_view text);
}
