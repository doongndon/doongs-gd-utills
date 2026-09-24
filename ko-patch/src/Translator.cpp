#include "Translator.hpp"

using namespace geode::prelude;

namespace kopatch {
    // 한글 음절 U+AC00..U+D7A3 은 UTF-8 에서 선두 바이트가 0xEA..0xED 이다.
    bool containsHangul(std::string_view text) {
        for (unsigned char byte : text) {
            if (byte >= 0xEA && byte <= 0xED) {
                return true;
            }
        }
        return false;
    }

    Translator& Translator::get() {
        static Translator instance;
        return instance;
    }

    void Translator::load() {
        m_table.clear();

        auto const path = Mod::get()->getResourcesDir() / "ko.json";
        auto json = file::readJson(path);
        if (json.isErr()) {
            log::error("could not read the Korean translation table");
            return;
        }

        auto const root = json.unwrapOrDefault();
        if (!root.isObject()) {
            log::error("the Korean translation table is not an object");
            return;
        }

        for (auto const& [english, korean] : root) {
            if (!korean.isString()) {
                continue;
            }
            auto text = korean.asString().unwrapOrDefault();
            if (english.empty() || text.empty()) {
                continue;
            }
            bool const isKorean = containsHangul(text);
            m_table.emplace(english, Entry{ .text = std::move(text), .korean = isKorean });
        }

        log::info("loaded {} Korean translations", m_table.size());
    }

    Entry const* Translator::find(std::string_view text) const {
        auto const found = m_table.find(text);
        return found == m_table.end() ? nullptr : &found->second;
    }
}
