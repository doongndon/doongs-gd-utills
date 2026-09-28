#pragma once

#include <string>

namespace kopatch::layoutlog {
    // 기기에서 글이 실제로 어디에 놓였는지 적어 둔다. 사진으로는 "치우쳤다"
    // 는 것까지만 보이고 왜 치우쳤는지는 보이지 않는다. 자를 대고 잰 숫자가
    // 있으면 한 번에 고칠 수 있다. '모은 글 복사' 가 이것도 함께 복사한다.
    //
    // 같은 줄은 한 번만, 모두 합쳐 몇십 줄까지만 담는다.
    void record(std::string line);

    // 담아 둔 것을 한 줄에 하나씩 이어 돌려준다. 없으면 빈 글.
    std::string dump();
}
