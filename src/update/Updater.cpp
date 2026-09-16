#include "Updater.hpp"

#include <Geode/Geode.hpp>
#include <Geode/loader/Dirs.hpp>
#include <Geode/loader/ModMetadata.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/web.hpp>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fmt/format.h>

using namespace geode::prelude;

namespace {
    // GitHub 의 "latest" 별칭. 릴리스를 새로 올려도 주소가 그대로라, 모드 안에
    // 버전 목록 같은 걸 들고 있을 필요가 없다.
    constexpr const char* DOWNLOAD_URL =
        "https://github.com/doongndon/doongs-gd-utills/releases/latest/download/doongndon.gd-utils.geode";

    std::atomic_bool RUNNING{ false };

    Ref<Notification> g_progress = nullptr;

    void finish(std::string text, NotificationIcon icon) {
        if (g_progress) {
            g_progress->hide();
            g_progress = nullptr;
        }
        Notification::create(text, icon, 4.f)->show();
        RUNNING = false;
    }

    // 받은 파일을 임시 자리에 먼저 두고 열어본 뒤, 멀쩡할 때만 진짜 자리로
    // 옮긴다. 다운로드가 깨졌는데 멀쩡히 깔려 있던 모드를 먼저 지워 버리면
    // 아무것도 없는 상태로 남기 때문이다.
    void install(ByteVector const& data) {
        auto* mod = Mod::get();
        auto const staging = dirs::getTempDir() / fmt::format("{}.update.geode", mod->getID());

        std::error_code ec;
        if (file::writeBinary(staging, data).isErr()) {
            finish("Could not save the download", NotificationIcon::Error);
            return;
        }

        auto const metadata = ModMetadata::createFromGeodeFile(staging);
        if (metadata.hasErrors() ||
            std::string_view(metadata.getID()) != std::string_view(mod->getID())) {
            std::filesystem::remove(staging, ec);
            finish("That download is not this mod", NotificationIcon::Error);
            return;
        }

        auto const version = metadata.getVersion();
        if (version <= mod->getVersion()) {
            std::filesystem::remove(staging, ec);
            finish(
                fmt::format("Already up to date ({})", mod->getVersion()),
                NotificationIcon::Info
            );
            return;
        }

        auto const target = dirs::getModsDir() / fmt::format("{}.geode", mod->getID());
        std::filesystem::remove(mod->getPackagePath(), ec);
        std::filesystem::rename(staging, target, ec);
        if (ec) {
            // 같은 디스크가 아니거나 이름 바꾸기가 막힌 경우에 대비한 우회로.
            if (file::writeBinary(target, data).isErr()) {
                finish("Could not install the update", NotificationIcon::Error);
                return;
            }
            std::filesystem::remove(staging, ec);
        }

        finish(
            fmt::format("Updated to {} - restart the game to apply", version),
            NotificationIcon::Success
        );
    }
}

namespace gdu::updater {
    void checkAndInstall() {
        if (RUNNING.exchange(true)) {
            return;
        }

        g_progress = Notification::create("Checking for updates", NotificationIcon::Loading, 0.f);
        g_progress->show();

        async::spawn(
            [] {
                return web::WebRequest()
                    .userAgent("doongs-gd-utils")
                    .timeout(std::chrono::seconds(120))
                    .get(DOWNLOAD_URL);
            },
            [](web::WebResponse response) {
                if (!response.ok()) {
                    finish(
                        fmt::format("Download failed (HTTP {})", response.code()),
                        NotificationIcon::Error
                    );
                    return;
                }
                install(std::move(response).data());
            }
        );
    }

    void listenForButton() {
        ButtonSettingPressedEventV3(Mod::get(), "check-updates").listen([](std::string_view) {
            checkAndInstall();
        }).leak();
    }
}
