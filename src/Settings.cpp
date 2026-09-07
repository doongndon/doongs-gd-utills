#include "Settings.hpp"

using namespace geode::prelude;

namespace gdu {
    Settings& Settings::get() {
        static Settings instance;
        return instance;
    }

    void Settings::reload() {
        auto* mod = Mod::get();

        hudEnabled = mod->getSettingValue<bool>("hud-enabled");
        hudPosition = mod->getSettingValue<std::string>("hud-position");
        hudScale = static_cast<float>(mod->getSettingValue<double>("hud-scale"));
        showAttempts = mod->getSettingValue<bool>("hud-attempts");
        showBest = mod->getSettingValue<bool>("hud-best");
        showPercent = mod->getSettingValue<bool>("hud-percent");
        showTimer = mod->getSettingValue<bool>("hud-timer");
        showCps = mod->getSettingValue<bool>("hud-cps");

        instantRestart = mod->getSettingValue<bool>("instant-restart");
        pauseInfo = mod->getSettingValue<bool>("pause-info");

        saveOnBackground = mod->getSettingValue<bool>("save-on-background");
        resumeButton = mod->getSettingValue<bool>("resume-button");

        ++revision;
    }
}
