#include <Geode/Geode.hpp>

#include <cstdint>

#include "Config.hpp"
#include "Updater.hpp"

using namespace geode::prelude;

swl::Config& swl::Config::get() {
    static Config config;
    return config;
}

void swl::Config::reload() {
    auto* mod = Mod::get();
    enabled = mod->getSettingValue<bool>("enabled");
    threshold = static_cast<float>(mod->getSettingValue<int64_t>("threshold-ms")) / 1000.f;
    tolerance = static_cast<double>(mod->getSettingValue<int64_t>("tolerance-ms"));
    showInfo = mod->getSettingValue<bool>("show-info");
}

$on_mod(Loaded) {
    swl::Config::get().reload();

    listenForAllSettingChanges([](std::string_view, std::shared_ptr<SettingV3>) {
        swl::Config::get().reload();
    });

    shared::updater::listenForButton("doongndon/doongs-gd-utills", "lag-latest");
}
