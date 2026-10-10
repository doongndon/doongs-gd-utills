#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>

namespace gdu::ui {
    // 도움말. 규칙 몇 줄과 오늘의 카드 목록(그림, 이름, 점수, 설명)을 위아래로 굴려 본다.
    // Geode 의 MDPopup 은 GD 글꼴로만 그려 한글이 빈칸이 되므로 직접 그린다.
    class HelpPopup : public geode::Popup {
    public:
        static HelpPopup* create();

    protected:
        bool init();
    };
}
