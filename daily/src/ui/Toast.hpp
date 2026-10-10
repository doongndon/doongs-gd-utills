#pragma once

#include <string>

namespace gdu::ui {
    // 화면 아래쪽에 잠깐 뜨는 알림. 장면이 바뀌어도 남는다(Geode 의 OverlayManager 위).
    // Geode 의 Notification 은 GD 글꼴로만 글을 그려 한글이 빈칸이 되므로 따로 둔다.
    // 새 알림이 오면 떠 있던 것은 바로 내린다.
    void toast(std::string const& text, bool good);
}
