#pragma once

#include <Geode/Geode.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace kopatch::colortags {
    // 글자 단위로 센 색 구간. GD 는 이것을 바이트로 세기 때문에 한글에서 어긋난다.
    struct Span {
        std::size_t start = 0;   // 글자 index
        std::size_t length = 0;  // 글자 수
        cocos2d::ccColor3B color{ 255, 255, 255 };
    };

    struct Parsed {
        std::string plain;
        std::vector<Span> spans;
    };

    bool hasTags(std::string_view text);

    // <cg>...</c> 를 떼어 내고, 어디부터 어디까지 무슨 색이었는지 글자 수로 적어 둔다.
    Parsed parse(std::string_view text);

    // 이미 만들어진 MultilineBitmapFont 의 글자마다 색을 입힌다. 생각한 모양이
    // 아니면 아무것도 하지 않는다. 그러면 색 없는 흰 글자로 남을 뿐이다.
    void apply(cocos2d::CCNode* node, std::string const& plain, std::vector<Span> const& spans);
}
