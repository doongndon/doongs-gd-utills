#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

namespace gdu {
    class DecoToolsPopup : public geode::Popup {
    public:
        static DecoToolsPopup* create(EditorUI* editorUI);

    private:
        bool init(EditorUI* editorUI);

        geode::TextInput* addInput(float x, float y, float width, char const* placeholder);
        void addLabel(float x, float y, char const* text, float scale, char const* font);
        void addButton(float x, float y, char const* text, cocos2d::SEL_MenuHandler handler);

        void onApplyScatter(cocos2d::CCObject*);
        void onApplyRamp(cocos2d::CCObject*);

        geode::Ref<EditorUI> m_editorUI;

        geode::TextInput* m_scatterPosition = nullptr;
        geode::TextInput* m_scatterRotation = nullptr;
        geode::TextInput* m_scatterScale = nullptr;

        geode::TextInput* m_rampRotationStart = nullptr;
        geode::TextInput* m_rampRotationEnd = nullptr;
        geode::TextInput* m_rampScaleStart = nullptr;
        geode::TextInput* m_rampScaleEnd = nullptr;
    };
}
