#pragma once

#include <Geode/Geode.hpp>

#include <optional>

namespace gdu::deco {
    // 값이 비어 있으면 그 항목은 건드리지 않는다.
    struct ScatterOptions {
        std::optional<float> position;  // +- 유닛
        std::optional<float> rotation;  // +- 도
        std::optional<float> scale;     // +- 퍼센트
    };

    // 선택한 오브젝트를 왼쪽에서 오른쪽 순서로 세워 놓고, 앞에서 뒤로 값을
    // 서서히 바꾼다. start 와 end 가 모두 있어야 그 항목이 적용된다.
    struct RampOptions {
        std::optional<float> rotationStart;
        std::optional<float> rotationEnd;
        std::optional<float> scaleStart;
        std::optional<float> scaleEnd;
    };

    // 바꾼 오브젝트 수. 선택이 비었거나 적용할 항목이 없으면 0.
    std::size_t scatter(EditorUI* editorUI, ScatterOptions const& options);
    std::size_t ramp(EditorUI* editorUI, RampOptions const& options);
}
