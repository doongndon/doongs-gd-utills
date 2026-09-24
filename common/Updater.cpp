#include "Updater.hpp"

#include <Geode/Geode.hpp>
#include <Geode/loader/Dirs.hpp>
#include <Geode/loader/Log.hpp>
#include <Geode/loader/ModMetadata.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/web.hpp>

#include <chrono>
#include <filesystem>
#include <fmt/format.h>

using namespace geode::prelude;

namespace {
    // 받는 일을 들고 있는 그릇. 이것을 놓아 버리면 받던 일도 함께 끊긴다.
    // 예전에는 spawn 이 돌려주는 손잡이를 그냥 버렸는데, 그러면 요청이 시작도
    // 못 하고 취소되고 콜백은 영영 불리지 않는다. 돌아가는 바퀴를 손에서
    // 놓아 버린 셈이라, 화면에는 돌아가는 표시만 남고 아무 일도 일어나지
    // 않았다. 프로그램이 살아 있는 동안 계속 있는 자리에 둔다.
    async::TaskHolder<web::WebResponse>& task() {
        static async::TaskHolder<web::WebResponse> holder;
        return holder;
    }

    Ref<Notification> g_progress = nullptr;

    void notify(std::string text, NotificationIcon icon) {
        if (g_progress) {
            g_progress->hide();
            g_progress = nullptr;
        }
        Notification::create(text, icon, 5.f)->show();
    }

    // 받은 파일을 임시 자리에 먼저 두고 열어본 뒤, 멀쩡할 때만 진짜 자리로
    // 옮긴다. 다운로드가 깨졌는데 멀쩡히 깔려 있던 모드를 먼저 지워 버리면
    // 아무것도 없는 상태로 남기 때문이다.
    void install(ByteVector const& data) {
        auto* mod = Mod::get();
        auto const staging = dirs::getTempDir() / fmt::format("{}.update.geode", mod->getID());

        log::info("update: downloaded {} bytes", data.size());

        std::error_code ec;
        if (file::writeBinary(staging, data).isErr()) {
            log::error("update: could not write {}", staging);
            notify("Could not save the download", NotificationIcon::Error);
            return;
        }

        auto const metadata = ModMetadata::createFromGeodeFile(staging);
        if (metadata.hasErrors() ||
            std::string_view(metadata.getID()) != std::string_view(mod->getID())) {
            log::error("update: the download is not {}", mod->getID());
            std::filesystem::remove(staging, ec);
            notify("That download is not this mod", NotificationIcon::Error);
            return;
        }

        auto const version = metadata.getVersion();
        if (version <= mod->getVersion()) {
            std::filesystem::remove(staging, ec);
            notify(
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
                log::error("update: could not install to {}", target);
                notify("Could not install the update", NotificationIcon::Error);
                return;
            }
            std::filesystem::remove(staging, ec);
        }

        log::info("update: installed {}", version);
        notify(
            fmt::format("Updated to {} - restart the game to apply", version),
            NotificationIcon::Success
        );
    }

    void checkAndInstall(std::string const& url) {
        // 이미 받고 있으면 그렇다고 말해 준다. 조용히 돌아가 버리면 단추가
        // 고장 난 것과 구별이 되지 않는다.
        if (task().isPending()) {
            Notification::create("Already checking", NotificationIcon::Info, 3.f)->show();
            return;
        }

        log::info("update: fetching {}", url);

        g_progress = Notification::create("Checking for updates", NotificationIcon::Loading, 0.f);
        g_progress->show();

        task().spawn(
            "check for updates",
            [url, agent = std::string(Mod::get()->getID())] {
                return web::WebRequest()
                    .userAgent(agent)
                    .timeout(std::chrono::seconds(120))
                    .get(url);
            },
            [](web::WebResponse response) {
                if (!response.ok()) {
                    log::error("update: HTTP {}", response.code());
                    notify(
                        fmt::format("Download failed (HTTP {})", response.code()),
                        NotificationIcon::Error
                    );
                    return;
                }
                install(std::move(response).data());
            }
        );
    }
}

namespace shared::updater {
    void listenForButton(std::string repository, std::string rollingTag) {
        auto url = fmt::format(
            "https://github.com/{}/releases/download/{}/{}.geode",
            repository, rollingTag, Mod::get()->getID()
        );

        ButtonSettingPressedEventV3(Mod::get(), "check-updates")
            .listen([url = std::move(url)](std::string_view) {
                checkAndInstall(url);
            })
            .leak();
    }
}
