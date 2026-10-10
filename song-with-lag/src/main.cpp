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
    showNotice = mod->getSettingValue<bool>("show-notice");
}

$on_mod(Loaded) {
    swl::Config::get().reload();

    listenForAllSettingChanges([](std::string_view, std::shared_ptr<SettingV3>) {
        swl::Config::get().reload();
    });

    shared::updater::listenForButton("doongndon/doongs-gd-utills", "lag-latest");
}
