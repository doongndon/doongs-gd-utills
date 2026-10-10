#include "Daily.hpp"

#include <Geode/utils/async.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <random>

#include <fmt/format.h>


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
                        .score = static_cast<int>(value["score"].asInt().unwrapOr(0)),
                        .bestPercent = static_cast<int>(value["bestPercent"].asInt().unwrapOr(0)),
                    };
                    if (value["cards"].isArray()) {
                        for (auto const& card : value["cards"]) {
                            loaded.tiers[id].cards.push_back(card.asString().unwrapOr(""));
                        }
                    }
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
            value.set("score", matjson::Value(progress.score));
            value.set("bestPercent", matjson::Value(progress.bestPercent));
            auto cards = matjson::Value::array();
            for (auto const& card : progress.cards) {
                cards.push(matjson::Value(card));
            }
            value.set("cards", cards);
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

    // 오늘의 도전 서버. 모두가 같은 순위표를 봐야 하므로 주소를 고정해 둔다.
    // 서버 코드는 저장소의 daily-server/ 에 있다.
    constexpr char const* SERVER = "https://doongs-gd-utills.onrender.com";

    std::string serverBase() {
        return SERVER;
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
        out.creator = level["creator"].asString().unwrapOr("");
        out.stars = static_cast<int>(level["stars"].asInt().unwrapOr(0));
        out.face = static_cast<int>(level["face"].asInt().unwrapOr(-1));
        out.feature = static_cast<int>(level["feature"].asInt().unwrapOr(0));
        out.length = static_cast<int>(level["length"].asInt().unwrapOr(-1));
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
                std::optional<int> score;
                if (auto value = row["score"].asInt(); value.isOk()) {
                    score = static_cast<int>(value.unwrapOr(0));
                }
                std::vector<std::string> cards;
                if (row["cards"].isArray()) {
                    for (auto const& card : row["cards"]) {
                        cards.push_back(card.asString().unwrapOr(""));
                    }
                }
                g_today->top.push_back({
                    .name = row["name"].asString().unwrapOr("?"),
                    .attempts = static_cast<int>(row["attempts"].asInt().unwrapOr(0)),
                    .score = score,
                    .cards = std::move(cards),
                });
            }
        }
        g_today->total = static_cast<int>(root["total"].asInt().unwrapOr(0));
        auto const& me = root["me"];
        g_today->myScore.reset();
        if (me.isObject()) {
            g_today->myRank = static_cast<int>(me["rank"].asInt().unwrapOr(0));
            if (auto score = me["score"].asInt(); score.isOk()) {
                g_today->myScore = static_cast<int>(score.unwrapOr(0));
            }
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
        if (submitTask().isPending()) {
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
        auto cards = matjson::Value::array();
        for (auto const& card : progress.cards) {
            cards.push(matjson::Value(card));
        }
        body.set("cards", cards);

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
            [tierID = *tierID, score = progress.score](web::WebResponse response) {
                auto& progress = stored().tiers[tierID];

                if (response.code() == 409 || response.code() == 400) {
                    // 보내는 사이에 날이 바뀌었거나(409), 서버가 받을 수 없는 기록이다(400).
                    // 다시 보내도 같은 답이 오므로 내려놓는다.
                    progress.submitted = true;
                    saveStored();
                    Notification::create(
                        response.code() == 409 ? "The daily challenge ended before your result arrived"
                                               : "The server could not accept this daily result",
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
                auto const what = fmt::format("Daily {} cleared with {} points", tierName(tierID), score);
                Notification::create(
                    rank ? fmt::format("{} - rank #{} of {}", what, *rank, total) : what,
                    NotificationIcon::Success, 5.f)->show();
                submitNext();
            }
        );
    }
}

namespace gdu::daily {
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
                Tier entry{
                    .id = value["id"].asString().unwrapOr(""),
                    .name = value["name"].asString().unwrapOr("?"),
                    .level = readLevel(value["level"]),
                };
                if (value["offers"].isArray()) {
                    for (auto const& offer : value["offers"]) {
                        std::vector<std::string> ids;
                        if (offer.isArray()) {
                            for (auto const& id : offer) {
                                ids.push_back(id.asString().unwrapOr(""));
                            }
                        }
                        entry.offers.push_back(std::move(ids));
                    }
                }
                today.tiers.push_back(std::move(entry));
            }
        }
        if (today.date.empty() || today.tiers.empty()) {
            return std::string("The server sent an empty challenge");
        }

        auto const& rogue = root["roguelike"];
        if (rogue.isObject()) {
            today.roguelike.base = static_cast<int>(rogue["base"].asInt().unwrapOr(100));
            if (rogue["milestones"].isArray()) {
                for (auto const& value : rogue["milestones"]) {
                    today.roguelike.milestones.push_back(static_cast<int>(value.asInt().unwrapOr(0)));
                }
            }
            if (rogue["cards"].isObject()) {
                for (auto const& [id, card] : rogue["cards"]) {
                    today.roguelike.cards[id] = Card{
                        .name = card["name"].asString().unwrapOr(id),
                        .text = card["text"].asString().unwrapOr(""),
                        .help = card["kind"].asString().unwrapOr("") == "help",
                        .points = static_cast<int>(card["points"].asInt().unwrapOr(0)),
                    };
                }
            }
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
        if (backgroundTask().isPending()) {
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

    std::optional<int> secondsLeft() {
        if (!today()) {
            return std::nullopt;
        }
        auto const left = std::chrono::duration_cast<std::chrono::seconds>(g_todayUntil - Clock::now()).count();
        return static_cast<int>(std::max<std::int64_t>(left, 0));
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

    bool clearedAnyToday() {
        auto const& current = today();
        if (!current) {
            return false;
        }
        return std::any_of(current->tiers.begin(), current->tiers.end(), [](Tier const& entry) {
            return progress(entry.id).clearedWith > 0;
        });
    }

    Card const* card(std::string const& id) {
        auto const& current = today();
        if (!current) {
            return nullptr;
        }
        auto const found = current->roguelike.cards.find(id);
        return found == current->roguelike.cards.end() ? nullptr : &found->second;
    }

    int tierFace(std::string_view tier) {
        if (tier == "easy") return 1;
        if (tier == "normal") return 2;
        if (tier == "hard") return 3;
        if (tier == "harder") return 4;
        if (tier == "insane") return 5;
        if (tier == "easy-demon") return 7;
        if (tier == "main") {
            // 메인 레벨 칸은 그날의 레벨 얼굴을 쓴다. 아직 모르면 N/A 얼굴.
            if (auto const* entry = daily::tier("main"); entry && entry->level) {
                return lookOf(*entry).face;
            }
        }
        return 0;
    }

    Look lookOf(Tier const& tier) {
        Look look;
        if (!tier.level) {
            look.face = tierFace(tier.id == "main" ? "" : tier.id);
            return look;
        }
        auto const& level = *tier.level;
        if (level.mainLevel) {
            // 메인 레벨은 GD 가 난이도와 별을 알고 있다. 레벨 내용은 읽지 않는다.
            if (auto* main = GameLevelManager::sharedState()->getMainLevel(level.id, true)) {
                int const difficulty = static_cast<int>(main->m_difficulty);
                look.face = std::clamp(difficulty, 0, 10);
                look.stars = main->m_stars.value();
            }
            look.creator = "RobTop";
            return look;
        }
        look.face = level.face >= 0 ? std::clamp(level.face, 0, 10) : tierFace(tier.id);
        look.feature = std::clamp(level.feature, 0, 4);
        look.stars = level.stars;
        look.creator = level.creator;
        if (level.length >= 0 && level.length <= 4) {
            look.length = std::string(GJGameLevel::lengthKeyToString(level.length));
        }
        return look;
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

    std::optional<int> attemptsToday(std::string const& tierID) {
        auto const* entry = tier(tierID);
        auto const saved = progress(tierID);
        if (!entry || !entry->level || saved.baseline < 0 || saved.levelID != entry->level->id) {
            return std::nullopt;
        }
        auto* manager = GameLevelManager::sharedState();
        GJGameLevel* level = entry->level->mainLevel
            ? manager->getMainLevel(entry->level->id, true)
            : manager->getSavedLevel(entry->level->id);
        if (!level) {
            return std::nullopt;
        }
        return std::max(0, level->m_attempts.value() - saved.baseline);
    }

    void notePercent(GJGameLevel* level, int percent) {
        auto const tierID = tierOf(level);
        if (!tierID) {
            return;
        }
        auto& progress = stored().tiers[*tierID];
        if (progress.baseline < 0 || percent <= progress.bestPercent) {
            return;
        }
        progress.bestPercent = std::clamp(percent, 0, 100);
        saveStored();
    }

    int partialScore(std::vector<std::string> const& cards) {
        auto const& current = today();
        int score = current ? current->roguelike.base : 100;
        for (auto const& id : cards) {
            if (auto const* entry = card(id)) {
                score += entry->points;
            }
        }
        return score;
    }

    std::optional<int> scoreOf(std::string const& tierID, std::vector<std::string> const& cards) {
        auto const& current = today();
        auto const* entry = tier(tierID);
        if (!current || !entry) {
            return std::nullopt;
        }
        auto const& rules = current->roguelike;
        auto const& offers = entry->offers;
        if (cards.size() != offers.size() || cards.empty()) {
            return std::nullopt;
        }
        int score = rules.base;
        for (std::size_t i = 0; i < cards.size(); ++i) {
            auto const& offer = offers[i];
            auto const card = rules.cards.find(cards[i]);
            if (std::find(offer.begin(), offer.end(), cards[i]) == offer.end() || card == rules.cards.end()) {
                return std::nullopt;
            }
            score += card->second.points;
        }
        return score;
    }

    void cleared(GJGameLevel* level, std::vector<std::string> const& cards) {
        auto const tierID = tierOf(level);
        if (!tierID) {
            return;
        }
        auto& progress = stored().tiers[*tierID];
        if (progress.baseline < 0) {
            return;
        }
        int const attempts = std::max(1, level->m_attempts.value() - progress.baseline);

        // 더 높은 점수로 다시 깨면 그 기록을 다시 보낸다. 점수가 같거나 낮으면 시도만
        // 더 쌓인 것이라 보낼 것이 없다.
        auto const score = scoreOf(*tierID, cards);
        if (!score || (progress.clearedWith > 0 && *score <= progress.score)) {
            return;
        }
        progress.cards = cards;
        progress.score = *score;
        progress.clearedWith = attempts;
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
        // 한 번 받아 둔 레벨이면 그 레벨 화면을 바로 연다. 아니면 레벨 번호로 검색한다.
        if (auto* saved = GameLevelManager::sharedState()->getSavedLevel(entry->level->id)) {
            director->pushScene(CCTransitionFade::create(0.5f, LevelInfoLayer::scene(saved, false)));
            return;
        }
        auto* search = GJSearchObject::create(SearchType::Search, std::to_string(entry->level->id));
        director->pushScene(CCTransitionFade::create(0.5f, LevelBrowserLayer::scene(search)));
    }
}
