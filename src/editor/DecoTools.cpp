#include "DecoTools.hpp"

#include <algorithm>
#include <random>
#include <vector>

using namespace geode::prelude;

namespace {
    std::mt19937& generator() {
        static std::mt19937 instance{ std::random_device{}() };
        return instance;
    }

    // -amount 와 +amount 사이의 값 하나.
    float jitter(float amount) {
        return std::uniform_real_distribution<float>(-amount, amount)(generator());
    }

    float lerp(float from, float to, float t) {
        return from + (to - from) * t;
    }

    // 되돌리기 지점을 먼저 찍어야 한다. UndoObject 는 "지금" 상태를 복사해 두는
    // 것이라, 오브젝트를 건드린 뒤에 부르면 바뀐 상태를 저장해 버린다.
    CCArray* beginEdit(EditorUI* editorUI) {
        if (!editorUI || !editorUI->m_editorLayer) {
            return nullptr;
        }

        auto* objects = editorUI->getSelectedObjects();
        if (!objects || objects->count() == 0) {
            return nullptr;
        }

        editorUI->m_editorLayer->addToUndoList(
            UndoObject::createWithTransformObjects(objects, UndoCommand::Transform), false
        );
        return objects;
    }
}

namespace gdu::deco {
    std::size_t scatter(EditorUI* editorUI, ScatterOptions const& options) {
        if (!options.position && !options.rotation && !options.scale) {
            return 0;
        }

        auto* objects = beginEdit(editorUI);
        if (!objects) {
            return 0;
        }

        for (auto* object : CCArrayExt<GameObject*>(objects)) {
            if (options.position) {
                editorUI->moveObject(object, { jitter(*options.position), jitter(*options.position) });
            }
            if (options.rotation) {
                object->setRRotation(object->getRotation() + jitter(*options.rotation));
            }
            if (options.scale) {
                // 가로세로를 같은 비율로 흔들어야 모양이 찌그러지지 않는다.
                float const factor = 1.f + jitter(*options.scale) / 100.f;
                object->setRScaleX(object->getRScaleX() * factor);
                object->setRScaleY(object->getRScaleY() * factor);
            }
        }

        return objects->count();
    }

    std::size_t ramp(EditorUI* editorUI, RampOptions const& options) {
        bool const doRotation = options.rotationStart && options.rotationEnd;
        bool const doScale = options.scaleStart && options.scaleEnd;
        if (!doRotation && !doScale) {
            return 0;
        }

        auto* objects = beginEdit(editorUI);
        if (!objects) {
            return 0;
        }

        // 선택 순서는 사용자가 클릭한 순서라 들쭉날쭉하다. 화면에 놓인 순서대로
        // 변해야 그라데이션처럼 보이므로 X 좌표로 줄을 세운다.
        std::vector<GameObject*> ordered;
        ordered.reserve(objects->count());
        for (auto* object : CCArrayExt<GameObject*>(objects)) {
            ordered.push_back(object);
        }
        std::sort(ordered.begin(), ordered.end(), [](GameObject* a, GameObject* b) {
            return a->getPositionX() < b->getPositionX();
        });

        float const last = static_cast<float>(ordered.size() - 1);
        for (std::size_t i = 0; i < ordered.size(); ++i) {
            float const t = last > 0.f ? static_cast<float>(i) / last : 0.f;

            if (doRotation) {
                ordered[i]->setRRotation(lerp(*options.rotationStart, *options.rotationEnd, t));
            }
            if (doScale) {
                ordered[i]->setRScale(lerp(*options.scaleStart, *options.scaleEnd, t));
            }
        }

        return ordered.size();
    }
}
