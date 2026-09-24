#include "KoreanFont.hpp"

#include <Geode/Geode.hpp>

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
}
