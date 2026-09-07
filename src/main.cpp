#include <Geode/Geode.hpp>

#include "Settings.hpp"

using namespace geode::prelude;

$on_mod(Loaded) {
    gdu::Settings::get().reload();

    listenForAllSettingChanges([](std::string_view, std::shared_ptr<SettingV3>) {
        gdu::Settings::get().reload();
    });
}
