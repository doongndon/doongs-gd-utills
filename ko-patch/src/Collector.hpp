#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace kopatch::collector {
    // 표에도 없고 기계 번역도 받지 못한 글을 모아 둔다. 설치된 모드 가운데
    // 소스를 공개하지 않은 것들이 있어, 그 글은 실제로 화면에 떠 봐야만 알 수
    // 있다. 게임을 한 바퀴 돌고 나면 그 목록이 곧 남은 할 일이 된다.
    void setEnabled(bool on);
    bool enabled();

    void note(std::string_view text);

    std::size_t count();

    // config/missing.txt 에 쓰고, 그 내용을 돌려준다.
    std::string flush();

    void listenForButton();
}
