#include "KoreanFont.hpp"

#include <Geode/Geode.hpp>

#include <string>
#include <vector>

using namespace geode::prelude;

namespace kopatch {
    std::string const& ownFont(bool pixel, bool gold) {
        static std::string const juaPlain = "jua.fnt"_spr;
        static std::string const juaGold = "jua-gold.fnt"_spr;
        static std::string const pixelPlain = "neodgm.fnt"_spr;
        static std::string const pixelGold = "neodgm-gold.fnt"_spr;

        if (pixel) {
            return gold ? pixelGold : pixelPlain;
        }
        return gold ? juaGold : juaPlain;
    }

    bool wantsGold(std::string_view fontFile) {
        return fontFile.find("gold") != std::string_view::npos;
    }

    namespace {
        bool g_splitting = false;
    }

    bool splittingText() {
        return g_splitting;
    }

    void setSplittingText(bool on) {
        g_splitting = on;
    }

    namespace {
        std::vector<std::string>& levelNames() {
            static std::vector<std::string> names;
            return names;
        }
    }

    LevelNameScope::LevelNameScope(std::string_view name) {
        levelNames().emplace_back(name);
    }

    LevelNameScope::~LevelNameScope() {
        levelNames().pop_back();
    }

    bool isLevelNameInScope(std::string_view text) {
        if (text.empty()) return false;
        for (auto const& name : levelNames()) {
            if (name == text) return true;
        }
        return false;
    }
}

