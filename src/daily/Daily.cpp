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

    // 이 기기의 오늘 기록. 날짜 하나와 난이도별 기록을 JSON 한 덩어리로 저장한다.
    struct Stored {
        std::string date;
        std::map<std::string, gdu::daily::Progress> tiers;
    };

    Stored& stored() {
        static Stored state = [] {
            Stored loaded;
            auto const text = Mod::get()->getSavedValue<std::string>("daily-progress", "");
            auto parsed = matjson::parse(text);
            if (!parsed) {
                return loaded;
            }
            auto const root = parsed.unwrapOrDefault();
            loaded.date = root["date"].asString().unwrapOr("");
            auto const& tiers = root["tiers"];
            if (tiers.isObject()) {
                for (auto const& [id, value] : tiers) {
                    loaded.tiers[id] = gdu::daily::Progress{
                        .levelID = static_cast<int>(value["level"].asInt().unwrapOr(0)),
                        .baseline = static_cast<int>(value["baseline"].asInt().unwrapOr(-1)),
                        .clearedWith = static_cast<int>(value["clearedWith"].asInt().unwrapOr(0)),
                        .submitted = value["submitted"].asBool().unwrapOr(false),
                    };
                }
            }
            return loaded;
        }();
        return state;
    }

    void saveStored() {
        auto const& state = stored();
        auto tiers = matjson::Value::object();
        for (auto const& [id, progress] : state.tiers) {
            auto value = matjson::Value::object();
            value.set("level", matjson::Value(progress.levelID));
            value.set("baseline", matjson::Value(progress.baseline));
            value.set("clearedWith", matjson::Value(progress.clearedWith));
            value.set("submitted", matjson::Value(progress.submitted));
            tiers.set(id, value);
        }
        auto root = matjson::Value::object();
        root.set("date", matjson::Value(state.date));
        root.set("tiers", tiers);
        Mod::get()->setSavedValue<std::string>("daily-progress", root.dump());
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

    std::optional<gdu::daily::Level> readLevel(matjson::Value const& level) {
        if (!level.isObject()) {
            return std::nullopt;
        }
        gdu::daily::Level out;
        out.id = static_cast<int>(level["id"].asInt().unwrapOr(0));
        out.mainLevel = level["kind"].asString().unwrapOr("main") != "online";
        out.name = level["name"].asString().unwrapOr("?");
        if (out.id <= 0) {
            return std::nullopt;
        }
        return out;
    }

    // 서버가 돌려준 순위표로 받아 둔 오늘의 도전을 고친다. 기록을 보낸 뒤의 답에도
    // 순위표가 실려 오므로 같은 길로 읽는다.
    void readBoard(matjson::Value const& root) {
        if (!g_today) {
            return;
        }
        g_today->boardTier = root["tier"].asString().unwrapOr(g_today->boardTier);
        g_today->top.clear();
        auto const& rows = root["top"];
        if (rows.isArray()) {
            for (auto const& row : rows) {
                g_today->top.push_back({
                    .name = row["name"].asString().unwrapOr("?"),
                    .attempts = static_cast<int>(row["attempts"].asInt().unwrapOr(0)),
                });
            }
        }
        g_today->total = static_cast<int>(root["total"].asInt().unwrapOr(0));
        auto const& me = root["me"];
        if (me.isObject()) {
            g_today->myRank = static_cast<int>(me["rank"].asInt().unwrapOr(0));
        }
        else {
            g_today->myRank.reset();
        }
    }

    // 아직 못 보낸 기록이 있는 난이도. 하나씩 차례로 보낸다.
    std::optional<std::string> pendingTier() {
        for (auto const& [id, progress] : stored().tiers) {
            if (progress.clearedWith > 0 && !progress.submitted) {
                return id;
            }
        }
        return std::nullopt;
    }

    std::string tierName(std::string const& id) {
        auto const* found = gdu::daily::tier(id);
        return found ? found->name : id;
    }

    void submitNext() {
        if (!gdu::daily::configured() || submitTask().isPending()) {
            return;
        }
        auto const tierID = pendingTier();
        if (!tierID) {
            return;
        }
        auto const progress = stored().tiers[*tierID];

        auto const who = player();
        auto playerJson = matjson::Value::object();
        playerJson.set("id", matjson::Value(who.id));
        playerJson.set("name", matjson::Value(who.name));
        auto body = matjson::Value::object();
        body.set("date", matjson::Value(stored().date));
        body.set("tier", matjson::Value(*tierID));
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
            [tierID = *tierID, attempts = progress.clearedWith](web::WebResponse response) {
                auto& progress = stored().tiers[tierID];

                if (response.code() == 409) {
                    // 보내는 사이에 날이 바뀌었다. 지난 날의 기록은 받아 주지 않는다.
                    progress.submitted = true;
                    saveStored();
                    Notification::create(
                        "The daily challenge ended before your result arrived",
                        NotificationIcon::Warning, 4.f)->show();
                    submitNext();
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
                saveStored();

                std::optional<int> rank;
                int total = 0;
                if (auto json = response.json(); json.isOk()) {
                    auto const root = json.unwrapOrDefault();
                    if (g_today && g_today->boardTier == tierID) {
                        readBoard(root);
                    }
                    if (root["me"].isObject()) {
                        rank = static_cast<int>(root["me"]["rank"].asInt().unwrapOr(0));
                    }
                    total = static_cast<int>(root["total"].asInt().unwrapOr(0));
                }
                Notification::create(
                    rank
                        ? fmt::format("Daily {} cleared in {} attempts - rank #{} of {}",
                                      tierName(tierID), attempts, *rank, total)
                        : fmt::format("Daily {} cleared in {} attempts", tierName(tierID), attempts),
                    NotificationIcon::Success, 5.f)->show();
                submitNext();
            }
        );
    }
}

namespace gdu::daily {
    bool configured() {
        return !serverBase().empty();
    }

    std::string selectedTier() {
        return Mod::get()->getSavedValue<std::string>("daily-tier", "main");
    }

    void selectTier(std::string const& tier) {
        Mod::get()->setSavedValue<std::string>("daily-tier", tier);
    }

    std::string todayUrl(std::string const& tier) {
        return fmt::format("{}/today?tier={}&player={}", serverBase(), tier, player().id);
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
        Today today;
        today.date = root["date"].asString().unwrapOr("");
        today.boardTier = root["tier"].asString().unwrapOr("main");
        auto const& tiers = root["tiers"];
        if (tiers.isArray()) {
            for (auto const& value : tiers) {
                today.tiers.push_back({
                    .id = value["id"].asString().unwrapOr(""),
                    .name = value["name"].asString().unwrapOr("?"),
                    .level = readLevel(value["level"]),
                });
            }
        }
        if (today.date.empty() || today.tiers.empty()) {
            return std::string("The server sent an empty challenge");
        }

        std::int64_t const secondsLeft = root["secondsLeft"].asInt().unwrapOr(0);
        g_today = std::move(today);
        g_todayUntil = Clock::now() + std::chrono::seconds(std::max<std::int64_t>(secondsLeft, 0));
        readBoard(root);

        // 날이 바뀌었으면 이 기기의 기록도 새로 시작한다. 같은 날이라도 서버가 그
        // 난이도의 레벨을 다르게 알려 주면(서버를 다시 띄운 경우 등) 그 기록은 버린다.
        auto& state = stored();
        bool changed = false;
        if (state.date != g_today->date) {
            state = Stored{ .date = g_today->date };
            changed = true;
        }
        for (auto const& entry : g_today->tiers) {
            auto found = state.tiers.find(entry.id);
            if (found != state.tiers.end() && entry.level && found->second.levelID != entry.level->id) {
                state.tiers.erase(found);
                changed = true;
            }
        }
        if (changed) {
            saveStored();
        }
        return std::nullopt;
    }

    void refreshInBackground() {
        if (!configured() || backgroundTask().isPending()) {
            return;
        }
        auto url = todayUrl(selectedTier());
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

    Tier const* tier(std::string const& id) {
        auto const& current = today();
        if (!current) {
            return nullptr;
        }
        for (auto const& entry : current->tiers) {
            if (entry.id == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    Progress progress(std::string const& tier) {
        auto const& tiers = stored().tiers;
        auto const found = tiers.find(tier);
        return found == tiers.end() ? Progress{} : found->second;
    }

    std::optional<std::string> tierOf(GJGameLevel* level) {
        auto const& current = today();
        if (!current || !level) {
            return std::nullopt;
        }
        int const id = level->m_levelID.value();
        bool const isMain = level->m_levelType == GJLevelType::Main;
        bool const isOnline = !isMain && level->m_levelType != GJLevelType::Editor;
        for (auto const& entry : current->tiers) {
            if (!entry.level || entry.level->id != id) {
                continue;
            }
            if (entry.level->mainLevel ? isMain : isOnline) {
                return entry.id;
            }
        }
        return std::nullopt;
    }

    void startPlaying(GJGameLevel* level) {
        auto const tierID = tierOf(level);
        if (!tierID) {
            return;
        }
        auto& progress = stored().tiers[*tierID];
        if (progress.baseline >= 0 && progress.levelID == level->m_levelID.value()) {
            return;
        }
        progress = Progress{ .levelID = level->m_levelID.value(), .baseline = level->m_attempts.value() };
        saveStored();
    }

    void cleared(GJGameLevel* level) {
        auto const tierID = tierOf(level);
        if (!tierID) {
            return;
        }
        auto& progress = stored().tiers[*tierID];
        // 처음 깬 것만 친다. 그 뒤에 다시 깨면 시도가 더 쌓였을 뿐이다.
        if (progress.baseline < 0 || progress.clearedWith > 0) {
            return;
        }
        progress.clearedWith = std::max(1, level->m_attempts.value() - progress.baseline);
        progress.submitted = false;
        saveStored();
        submitNext();
    }

    void resubmitPending() {
        submitNext();
    }

    void open(std::string const& tierID) {
        auto const* entry = tier(tierID);
        if (!entry || !entry->level) {
            return;
        }
        auto* director = CCDirector::sharedDirector();
        if (entry->level->mainLevel) {
            // 메인 레벨은 레벨 고르는 화면의 그 쪽으로 간다. 잠긴 레벨을 몰래 열지 않는다.
            director->replaceScene(CCTransitionFade::create(
                0.5f, LevelSelectLayer::scene(entry->level->id - 1)));
            return;
        }
        auto* search = GJSearchObject::create(SearchType::Search, std::to_string(entry->level->id));
        director->pushScene(CCTransitionFade::create(0.5f, LevelBrowserLayer::scene(search)));
    }
}
