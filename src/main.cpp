#include <Geode/Geode.hpp>

#include "Settings.hpp"
#include "Updater.hpp"

using namespace geode::prelude;

$on_mod(Loaded) {
    gdu::Settings::get().reload();

    listenForAllSettingChanges([](std::string_view, std::shared_ptr<SettingV3>) {
        gdu::Settings::get().reload();
    });

    shared::updater::listenForButton("doongndon/doongs-gd-utills", "utils-latest");
}
