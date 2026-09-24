#include <Geode/Geode.hpp>

#include "Translator.hpp"

using namespace geode::prelude;

$on_mod(Loaded) {
    auto* mod = Mod::get();
    auto& translator = kopatch::Translator::get();

    translator.load();
    translator.setEnabled(mod->getSettingValue<bool>("enabled"));
    translator.setOwnFont(mod->getSettingValue<bool>("own-font"));

    listenForSettingChanges<bool>("enabled", [](bool enabled) {
        kopatch::Translator::get().setEnabled(enabled);
    });
    listenForSettingChanges<bool>("own-font", [](bool own) {
        kopatch::Translator::get().setOwnFont(own);
    });
}
