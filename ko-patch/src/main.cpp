#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include <string>

#include "Collector.hpp"
#include "Gemini.hpp"
#include "Translator.hpp"
#include "Updater.hpp"

using namespace geode::prelude;

// 지금 화면에 이미 적혀 있는 글자를 훅에 한 번 더 태운다.
static void retranslateScene() {
    auto* scene = CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return;

    auto walk = [](auto&& self, CCNode* node) -> void {
        if (!node) return;
        if (auto* label = typeinfo_cast<CCLabelBMFont*>(node)) {
            if (char const* text = label->getString(); text && *text) {
                // 라벨이 제 안에 들고 있는 글자를 그대로 도로 넣으면 저를
                // 지우며 저를 읽는 꼴이 된다. 한 번 베껴 두고 넣는다.
                std::string const copy(text);
                label->setString(copy.c_str());
            }
        }
        auto* children = node->getChildren();
        if (!children) return;
        for (auto* child : CCArrayExt<CCNode*>(children)) {
            self(self, child);
        }
    };
    walk(walk, scene);
}

$on_mod(Loaded) {
    auto* mod = Mod::get();
    auto& translator = kopatch::Translator::get();

    translator.load();
    translator.setEnabled(mod->getSettingValue<bool>("enabled"));
    translator.setOwnFont(mod->getSettingValue<bool>("own-font"));
    translator.setPixelFont(mod->getSettingValue<std::string>("font") == "dunggeunmo");

    listenForSettingChanges<bool>("enabled", [](bool enabled) {
        kopatch::Translator::get().setEnabled(enabled);
    });
    listenForSettingChanges<bool>("own-font", [](bool own) {
        kopatch::Translator::get().setOwnFont(own);
    });
    listenForSettingChanges<std::string>("font", [](std::string const& font) {
        kopatch::Translator::get().setPixelFont(font == "dunggeunmo");
    });

    auto applyGemini = [] {
        auto* mod = Mod::get();
        kopatch::gemini::configure(
            mod->getSettingValue<bool>("gemini-enabled"),
            mod->getSettingValue<std::string>("gemini-key"),
            mod->getSettingValue<std::string>("gemini-model")
        );
    };
    kopatch::gemini::load();
    applyGemini();

    listenForSettingChanges<bool>("gemini-enabled", [applyGemini](bool) { applyGemini(); });
    listenForSettingChanges<std::string>("gemini-key", [applyGemini](std::string const&) { applyGemini(); });
    listenForSettingChanges<std::string>("gemini-model", [applyGemini](std::string const&) { applyGemini(); });

    kopatch::collector::setEnabled(mod->getSettingValue<bool>("collect-enabled"));
    listenForSettingChanges<bool>("collect-enabled", [](bool on) {
        kopatch::collector::setEnabled(on);
    });
    kopatch::collector::listenForButton();

    translator.protectModNames();

    shared::updater::listenForButton("doongndon/doongs-gd-utills", "ko-latest");

    // 로딩 화면은 우리보다 먼저 글자를 올려 둔다. 모드는 141 개가 차례로
    // 불려 오고 우리는 그 줄의 가운데쯤에 있는데, 그때는 이미 팁이 뽑혀
    // 라벨에 적힌 뒤다. 훅은 앞으로 올라올 글자만 잡으므로 이미 적힌 글은
    // 영영 영어로 남는다.
    //
    // 그래서 우리가 켜지는 순간 화면에 있는 글자를 한 번 다시 올린다.
    // 같은 글을 그대로 setString 하는 것뿐이라 훅이 그때 번역한다.
    retranslateScene();
}

// 우리 모드가 켜질 때는 다른 모드가 아직 다 불려 오지 않았을 수 있다. 메뉴가
// 처음 뜰 때 한 번 더 모아 둔다. 모드 목록은 거기서부터 열리기 때문이다.
class $modify(KoreanMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) {
            return false;
        }

        static bool collected = false;
        if (!collected) {
            collected = true;
            kopatch::Translator::get().protectModNames();
        }
        return true;
    }
};
