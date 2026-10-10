#include "Daily.hpp"

#include <Geode/utils/async.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <random>

#include <fmt/format.h>

#include "Settings.hpp"

using namespace geode::prelude;

namespace {
    using Clock = std::chrono::steady_clock;

    std::optional<gdu::daily::Today> g_today;
    // 받아 둔 오늘의 도전이 언제까지 오늘인지. 자정을 넘기면 다시 물어봐야 한다.
    Clock::time_point g_todayUntil{};

    gdu::daily::Progress& progressRef() {
        static gdu::daily::Progress progress = [] {
            auto* mod = Mod::get();
            gdu::daily::Progress loaded;
            loaded.date = mod->getSavedValue<std::string>("daily-date", "");
            loaded.levelID = mod->getSavedValue<int>("daily-level", 0);
            loaded.baseline = mod->getSavedValue<int>("daily-baseline", -1);
            loaded.clearedWith = mod->getSavedValue<int>("daily-cleared-with", 0);
            loaded.submitted = mod->getSavedValue<bool>("daily-submitted", false);
            return loaded;
        }();
        return progress;
    }

    void saveProgress() {
        auto const& progress = progressRef();
        auto* mod = Mod::get();
        mod->setSavedValue<std::string>("daily-date", progress.date);
        mod->setSavedValue<int>("daily-level", progress.levelID);
        mod->setSavedValue<int>("daily-baseline", progress.baseline);
        mod->setSavedValue<int>("daily-cleared-with", progress.clearedWith);
        mod->setSavedValue<bool>("daily-submitted", progress.submitted);
    }

    // 계정이 없는 사람을 가리는 이름표. 처음 한 번 아무렇게나 지어 두고 계속 쓴다.
    std::string deviceID() {
        auto* mod = Mod::get();
        auto id = mod->getSavedValue<std::string>("daily-device", "");
        if (!id.empty()) {
            return id;
        }

        constexpr char LETTERS[] = "abcdefghijklmnopqrstuvwxyz0123456789";
        std::random_device seed;
        std::mt19937 random(seed());
        std::uniform_int_distribution<std::size_t> pick(0, sizeof(LETTERS) - 2);
        for (int i = 0; i < 16; ++i) {
            id += LETTERS[pick(random)];
        }
        mod->setSavedValue<std::string>("daily-device", id);
        return id;
    }

    struct Player {
        std::string id;
        std::string name;
    };

    // 로그인한 사람은 계정 번호로 가린다. 다른 기기에서 해도 한 사람으로 센다.
    Player player() {
        auto* account = GJAccountManager::sharedState();
        if (account && account->m_accountID > 0) {
            std::string name(account->m_username);
            return { fmt::format("acc:{}", account->m_accountID), name.empty() ? "Player" : name };
        }

        std::string name;
        if (auto* manager = GameManager::sharedState()) {
            name = std::string(manager->m_playerName);
        }
        return { fmt::format("dev:{}", deviceID()), name.empty() ? "Player" : name };
    }

    std::string serverBase() {
        auto base = gdu::Settings::get().dailyServer;
        while (!base.empty() && (base.back() == '/' || base.back() == ' ')) {
            base.pop_back();
        }
        while (!base.empty() && base.front() == ' ') {
            base.erase(base.begin());
        }
        return base;
    }

    async::TaskHolder<web::WebResponse>& backgroundTask() {
        static async::TaskHolder<web::WebResponse> holder;
        return holder;
    }

    async::TaskHolder<web::WebResponse>& submitTask() {
        static async::TaskHolder<web::WebResponse> holder;
        return holder;
    }

    std::vector<gdu::daily::Row> readRows(matjson::Value const& rows) {
        std::vector<gdu::daily::Row> out;
        if (!rows.isArray()) {
            return out;
        }
        for (auto const& row : rows) {
            out.push_back({
                .name = row["name"].asString().unwrapOr("?"),
                .attempts = static_cast<int>(row["attempts"].asInt().unwrapOr(0)),
            });
        }
        return out;
    }

    // 서버가 돌려준 순위표로 받아 둔 오늘의 도전을 고친다. 기록을 보낸 뒤의 답에도
    // 순위표가 실려 오므로 같은 길로 읽는다.
    void readBoard(matjson::Value const& root) {
        if (!g_today) {
            return;
        }
        g_today->top = readRows(root["top"]);
        g_today->total = static_cast<int>(root["total"].asInt().unwrapOr(0));
        auto const& me = root["me"];
        if (me.isObject()) {
            g_today->myRank = static_cast<int>(me["rank"].asInt().unwrapOr(0));
            g_today->myAttempts = static_cast<int>(me["attempts"].asInt().unwrapOr(0));
        }
        else {
            g_today->myRank.reset();
            g_today->myAttempts.reset();
        }
    }

    void submit() {
        auto const& progress = progressRef();
        if (!gdu::daily::configured() || progress.clearedWith <= 0 || progress.submitted) {
            return;
        }
        if (submitTask().isPending()) {
            return;
        }

        auto const who = player();
        auto body = matjson::Value::object();
        auto playerJson = matjson::Value::object();
        playerJson.set("id", matjson::Value(who.id));
        playerJson.set("name", matjson::Value(who.name));
        body.set("date", matjson::Value(progress.date));
        body.set("levelId", matjson::Value(progress.levelID));
        body.set("player", playerJson);
        body.set("attempts", matjson::Value(progress.clearedWith));

        auto url = serverBase() + "/result";
        submitTask().spawn(
            "daily result",
            [url = std::move(url), body = std::move(body), agent = std::string(Mod::get()->getID())] {
                return web::WebRequest()
                    .userAgent(agent)
                    .bodyJSON(body)
                    .timeout(std::chrono::seconds(20))
                    .post(url);
            },
            [](web::WebResponse response) {
                auto& progress = progressRef();

                if (response.code() == 409) {
                    // 보내는 사이에 날이 바뀌었다. 지난 날의 기록은 받아 주지 않는다.
                    progress.submitted = true;
                    saveProgress();
                    Notification::create(
                        "The daily challenge ended before your result arrived",
                        NotificationIcon::Warning, 4.f)->show();
                    return;
                }
                if (!response.ok()) {
                    log::warn("daily: could not send the result (HTTP {})", response.code());
                    Notification::create(
                        "Could not send your daily result - it will retry next time you open Daily",
                        NotificationIcon::Error, 4.f)->show();
                    return;
                }

                progress.submitted = true;
                saveProgress();

                auto json = response.json();
                if (json.isOk()) {
                    readBoard(json.unwrapOrDefault());
                }
                auto const& today = gdu::daily::today();
                if (today && today->myRank) {
                    Notification::create(
                        fmt::format("Daily cleared in {} attempts - rank #{} of {}",
                                    progress.clearedWith, *today->myRank, today->total),
                        NotificationIcon::Success, 5.f)->show();
                }
                else {
                    Notification::create(
                        fmt::format("Daily cleared in {} attempts", progress.clearedWith),
                        NotificationIcon::Success, 5.f)->show();
                }
            }
        );
    }
}

namespace gdu::daily {
    bool configured() {
        return !serverBase().empty();
    }

    std::string todayUrl() {
        return fmt::format("{}/today?player={}", serverBase(), player().id);
    }

    std::optional<std::string> acceptToday(web::WebResponse const& response) {
        if (!response.ok()) {
            return fmt::format("The server did not answer (HTTP {})", response.code());
        }
        auto json = response.json();
        if (json.isErr()) {
            return std::string("The server sent something unreadable");
        }

        auto const root = json.unwrapOrDefault();
        auto const& level = root["level"];
        Today today;
        today.date = root["date"].asString().unwrapOr("");
        today.levelID = static_cast<int>(level["id"].asInt().unwrapOr(0));
        today.mainLevel = level["kind"].asString().unwrapOr("main") != "online";
        today.levelName = level["name"].asString().unwrapOr("?");
        if (today.date.empty() || today.levelID <= 0) {
            return std::string("The server sent an empty challenge");
        }

        std::int64_t const secondsLeft = root["secondsLeft"].asInt().unwrapOr(0);
        g_today = std::move(today);
        g_todayUntil = Clock::now() + std::chrono::seconds(std::max<std::int64_t>(secondsLeft, 0));
        readBoard(root);

        // 날이 바뀌었거나 레벨이 바뀌었으면 이 기기의 기록도 새로 시작한다.
        auto& progress = progressRef();
        if (progress.date != g_today->date || progress.levelID != g_today->levelID) {
            progress = Progress{ .date = g_today->date, .levelID = g_today->levelID };
            saveProgress();
        }
        return std::nullopt;
    }

    void refreshInBackground() {
        if (!configured() || backgroundTask().isPending()) {
            return;
        }
        auto url = todayUrl();
        backgroundTask().spawn(
            "daily today",
            [url = std::move(url), agent = std::string(Mod::get()->getID())] {
                return web::WebRequest()
                    .userAgent(agent)
                    .timeout(std::chrono::seconds(20))
                    .get(url);
            },
            [](web::WebResponse response) {
                if (auto const error = acceptToday(response)) {
                    log::info("daily: {}", *error);
                    return;
                }
                resubmitPending();
            }
        );
    }

    std::optional<Today> const& today() {
        // 자정을 넘긴 오늘의 도전은 어제의 것이다. 모르는 것으로 친다.
        if (g_today && Clock::now() >= g_todayUntil) {
            g_today.reset();
        }
        return g_today;
    }

    Progress const& progress() {
        return progressRef();
    }

    bool isTodaysLevel(GJGameLevel* level) {
        auto const& current = today();
        if (!current || !level || level->m_levelID.value() != current->levelID) {
            return false;
        }
        if (current->mainLevel) {
            return level->m_levelType == GJLevelType::Main;
        }
        return level->m_levelType != GJLevelType::Main && level->m_levelType != GJLevelType::Editor;
    }

    void startPlaying(GJGameLevel* level) {
        if (!isTodaysLevel(level)) {
            return;
        }
        auto& progress = progressRef();
        if (progress.baseline >= 0) {
            return;
        }
        progress.baseline = level->m_attempts.value();
        saveProgress();
    }

    std::optional<int> attemptsSoFar(GJGameLevel* level) {
        auto const& progress = progressRef();
        if (!isTodaysLevel(level) || progress.baseline < 0) {
            return std::nullopt;
        }
        return std::max(1, level->m_attempts.value() - progress.baseline);
    }

    void cleared(GJGameLevel* level) {
        auto const attempts = attemptsSoFar(level);
        auto& progress = progressRef();
        // 처음 깬 것만 친다. 그 뒤에 다시 깨면 시도가 더 쌓였을 뿐이다.
        if (!attempts || progress.clearedWith > 0) {
            return;
        }
        progress.clearedWith = *attempts;
        progress.submitted = false;
        saveProgress();
        submit();
    }

    void resubmitPending() {
        submit();
    }

    void open() {
        auto const& current = today();
        if (!current) {
            return;
        }
        auto* director = CCDirector::sharedDirector();
        if (current->mainLevel) {
            // 메인 레벨은 레벨 고르는 화면의 그 쪽으로 간다. 잠긴 레벨을 몰래 열지 않는다.
            director->replaceScene(CCTransitionFade::create(
                0.5f, LevelSelectLayer::scene(current->levelID - 1)));
            return;
        }
        auto* search = GJSearchObject::create(SearchType::Search, std::to_string(current->levelID));
        director->pushScene(CCTransitionFade::create(0.5f, LevelBrowserLayer::scene(search)));
    }
}
