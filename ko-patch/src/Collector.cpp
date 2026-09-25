#include "Collector.hpp"

#include <Geode/Geode.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/utils/StringMap.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/general.hpp>

#include <algorithm>
#include <cctype>
#include <vector>

#include <fmt/format.h>

#include "Translator.hpp"

using namespace geode::prelude;

namespace {
    constexpr std::size_t LIMIT = 4000;      // 한 번에 담아 둘 줄 수
    constexpr std::size_t FLUSH_EVERY = 20;  // 이만큼 새로 들어오면 파일에 쓴다
    constexpr std::size_t CLIPBOARD_MAX = 15000;

    bool g_enabled = false;
    std::size_t g_sinceWrite = 0;

    geode::utils::StringSet& seen() {
        static geode::utils::StringSet set;
        return set;
    }

    std::filesystem::path path() {
        return Mod::get()->getConfigDir() / "missing.txt";
    }

    // 화면에 뜨는 글만 남긴다. 저장 열쇠나 그림 이름, 주소 조각은 번역할 것이
    // 아니라서, 목록에 섞이면 정작 볼 것이 묻힌다.
    bool worthKeeping(std::string_view text) {
        if (text.size() < 2 || text.size() > 200) return false;
        if (kopatch::containsHangul(text)) return false;
        if (text.starts_with("http") || text.starts_with("&") || text.starts_with("--")) {
            return false;
        }

        bool letter = false;
        bool space = false;
        for (unsigned char byte : text) {
            if (byte < 0x20 || byte > 0x7E) return false;  // 영어 아닌 것은 우리 몫이 아니다
            if (std::isalpha(byte)) letter = true;
            if (byte == ' ') space = true;
        }
        if (!letter) return false;

        // 띄어쓰기가 없는데 / 나 _ 가 들어 있으면 사람이 읽을 글이 아니다.
        if (!space && (text.find('/') != std::string_view::npos
                       || text.find('_') != std::string_view::npos)) {
            return false;
        }
        return true;
    }
}

namespace kopatch::collector {
    void setEnabled(bool on) { g_enabled = on; }
    bool enabled() { return g_enabled; }

    std::size_t count() { return seen().size(); }

    void note(std::string_view text) {
        if (!g_enabled || seen().size() >= LIMIT) return;
        if (!worthKeeping(text)) return;
        if (Translator::get().isProtected(text)) return;  // 남의 모드 이름
        if (!seen().emplace(std::string(text)).second) return;

        if (++g_sinceWrite >= FLUSH_EVERY) {
            flush();
        }
    }

    std::string flush() {
        g_sinceWrite = 0;

        std::vector<std::string_view> lines;
        lines.reserve(seen().size());
        for (auto const& line : seen()) {
            lines.push_back(line);
        }
        std::ranges::sort(lines);

        std::string out;
        for (auto const& line : lines) {
            out += line;
            out += '\n';
        }

        if (auto const written = file::writeStringSafe(path(), out); written.isErr()) {
            log::warn("could not write {}: {}", path(), written.unwrapErr());
        }
        return out;
    }

    void listenForButton() {
        ButtonSettingPressedEventV3(Mod::get(), "collect-copy")
            .listen([](std::string_view) {
                auto const text = flush();
                if (text.empty()) {
                    Notification::create(
                        "Nothing collected yet", NotificationIcon::Info, 4.f)->show();
                    return;
                }

                // 클립보드에 다 담기지 않을 만큼 길 수도 있다. 파일에는 전부 있다.
                std::string_view clipped(text);
                if (clipped.size() > CLIPBOARD_MAX) {
                    auto const cut = clipped.rfind('\n', CLIPBOARD_MAX);
                    clipped = clipped.substr(0, cut == std::string_view::npos ? 0 : cut + 1);
                }
                geode::utils::clipboard::write(std::string(clipped));

                auto const copied = std::ranges::count(clipped, '\n');
                Notification::create(
                    fmt::format("Copied {} of {} lines - all of them are in config/missing.txt",
                                copied, count()),
                    NotificationIcon::Success, 5.f)->show();
            })
            .leak();
    }
}
