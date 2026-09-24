#include <Geode/Geode.hpp>

#include "Translator.hpp"

using namespace geode::prelude;

$on_mod(Loaded) {
    auto& translator = kopatch::Translator::get();
    translator.load();
    translator.setEnabled(Mod::get()->getSettingValue<bool>("enabled"));

    listenForSettingChanges<bool>("enabled", [](bool enabled) {
        kopatch::Translator::get().setEnabled(enabled);
    });
}
