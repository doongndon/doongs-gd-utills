#include <Geode/Geode.hpp>

#include "Updater.hpp"

using namespace geode::prelude;

$on_mod(Loaded) {
    shared::updater::listenForButton("doongndon/doongs-gd-utills", "daily-latest");
}
