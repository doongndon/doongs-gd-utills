#pragma once

namespace gdu::updater {
    // 최신 릴리스를 받아서 깔고, 다시 켜라고 알려준다.
    void checkAndInstall();

    // 설정 화면의 버튼 눌림을 받는다. 모드 로드 때 한 번 부른다.
    void listenForButton();
}
