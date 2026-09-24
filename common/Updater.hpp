#pragma once

#include <string>

// 두 모드가 같은 코드를 쓴다. 받아서 깔고 다시 켜라고 알리는 일은 어느 모드든
// 똑같기 때문이다.
namespace shared::updater {
    // 모드 설정의 "check-updates" 버튼 눌림을 받는다. 모드 로드 때 한 번 부른다.
    //
    // rollingTag 는 언제나 최신 파일을 가리키도록 릴리스 워크플로가 옮겨 주는
    // 태그다. GitHub 의 "latest" 별칭은 저장소 전체에서 가장 최근 릴리스 하나만
    // 가리켜서, 저장소에 모드가 둘 있으면 엉뚱한 릴리스를 짚는다.
    void listenForButton(std::string repository, std::string rollingTag);
}
