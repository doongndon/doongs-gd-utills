#include <Geode/Geode.hpp>

#include "Translator.hpp"
#include "Updater.hpp"

using namespace geode::prelude;

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

    shared::updater::listenForButton("doongndon/doongs-gd-utills", "ko-latest");
}
