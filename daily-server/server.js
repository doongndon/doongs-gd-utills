// Doong's GD Utils 오늘의 도전 서버
//
// 하는 일
// - 날마다 난이도별로 레벨 하나씩을 고른다. 메인 레벨 칸은 levels.json 에서, 나머지
//   칸(Easy ~ Easy Demon)은 GD 서버에서 그 난이도의 레이팅된 레벨 가운데 하나를
//   무작위로 골라 온다. 한 번 고른 레벨은 그날 하루 바뀌지 않는다.
// - 모든 칸이 로그라이크다. 25%, 50%, 75% 에서 카드를 한 장씩 고르며 깨고, 점수는
//   100 + 고른 카드의 점수다. 카드 묶음은 날짜와 칸마다 정해져 그 칸을 하는 사람은
//   모두 같은 카드를 받는다.
// - 칸마다 따로 순위를 매긴다. 점수가 높은 순, 같으면 적은 시도 순.
// - 한 사람은 칸마다 하루에 한 줄. 더 좋은 기록으로 다시 깨면 그 기록으로 바꾼다.
//
// 기록은 정직함에 기댄다. 모드가 연습 모드나 시작 위치로 깬 것은 보내지 않지만,
// 서버는 누가 보냈는지 확인할 방법이 없다. 상금을 거는 대회가 아니라 재미로
// 견주는 판이다.
//
// 바깥 패키지는 쓰지 않는다. node 만 있으면 돈다.
const http = require("http");
const https = require("https");
const fs = require("fs");
const path = require("path");

// 하루가 바뀌는 시각. 쓰는 사람이 대부분 한국에 있으므로 한국 시간 자정이 기본이다.
const UTC_OFFSET_HOURS = Number(process.env.UTC_OFFSET_HOURS ?? 9);
const TOP = 10;
const MAX_NAME = 20;
const MAX_ATTEMPTS = 1000000;
const MAX_BODY = 2048;
// 지난 날의 기록은 이만큼만 들고 있는다. 오래 켜 둬도 메모리가 늘지 않게.
const KEEP_DAYS = 7;
// 한 주소에서 1분에 보낼 수 있는 기록 수. 실수든 장난이든 서버를 못 쓰게 만들지 않게.
const POSTS_PER_MINUTE = 30;
// GD 서버에서 고르기에 실패하면 이만큼 기다렸다가 다시 해 본다. 매 요청마다 두드리지 않게.
const RETRY_AFTER_MS = 10 * 60 * 1000;
// 무작위로 고를 쪽의 범위. GD 서버가 아주 뒤쪽 쪽은 잘 주지 않을 수 있어 앞쪽에서만 고른다.
const MAX_PAGES = 200;
const PLAYER_ID = /^(acc|dev):[A-Za-z0-9]{1,32}$/;

// 고를 수 있는 난이도. diff, demonFilter 는 GD 서버의 레벨 검색 값이다.
// 데몬은 너무 어려운 것이 걸리지 않게 이지 데몬만 둔다.
const TIERS = [
    { id: "main", name: "Main levels" },
    { id: "easy", name: "Easy", diff: "1" },
    { id: "normal", name: "Normal", diff: "2" },
    { id: "hard", name: "Hard", diff: "3" },
    { id: "harder", name: "Harder", diff: "4" },
    { id: "insane", name: "Insane", diff: "5" },
    { id: "easy-demon", name: "Easy Demon", diff: "-2", demonFilter: "1" },
];

// 로그라이크 칸의 카드. 25%, 50%, 75% 에서 세 장 가운데 한 장을 고른다. 도움 카드는
// 점수를 깎고 저주 카드는 점수를 올린다. 효과는 모드가 id 를 보고 건다. 이름과
// 설명은 모드가 그대로 화면에 띄운다.
const CARDS = {
    shield: { kind: "help", points: -30, name: "Shield", text: "Survive one death, then 1s of safety" },
    slow: { kind: "help", points: -20, name: "Slow", text: "The game runs at 0.85x speed" },
    mirror: { kind: "curse", points: 25, name: "Mirror", text: "The screen is flipped" },
    mini: { kind: "curse", points: 15, name: "Mini", text: "You turn mini until a portal says otherwise" },
    fast: { kind: "curse", points: 30, name: "Rush", text: "The game runs at 1.2x speed" },
    dark: { kind: "curse", points: 20, name: "Dark", text: "The screen goes dim" },
    shake: { kind: "curse", points: 15, name: "Quake", text: "The screen shakes" },
    invisible: { kind: "curse", points: 35, name: "Ghost", text: "Your icon disappears" },
    mute: { kind: "curse", points: 10, name: "Silence", text: "The music stops" },
    flash: { kind: "curse", points: 25, name: "Blackout", text: "The screen blacks out now and then" },
};
const MILESTONES = [25, 50, 75];
const BASE_SCORE = 100;

// 수 하나로 이어지는 주사위. 같은 씨앗이면 언제나 같은 수가 나온다(mulberry32).
function seeded(seed) {
    let state = seed >>> 0;
    return () => {
        state = (state + 0x6d2b79f5) >>> 0;
        let t = state;
        t = Math.imul(t ^ (t >>> 15), t | 1);
        t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
        return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
}

// 그날의 카드 묶음. 지점마다 도움 한 장, 저주 두 장. 저주는 하루 안에서 겹치지 않는다.
// 날짜만 같으면 누구에게나 같은 묶음이 나온다.
function offersFor(date, tierId) {
    const roll = seeded(hash(`${date}:${tierId}:cards`));
    const take = (pool) => pool.splice(Math.floor(roll() * pool.length), 1)[0];
    const helps = Object.keys(CARDS).filter((id) => CARDS[id].kind === "help");
    const curses = Object.keys(CARDS).filter((id) => CARDS[id].kind === "curse");
    return MILESTONES.map(() => {
        const help = helps[Math.floor(roll() * helps.length)];
        const offer = [help, take(curses), take(curses)];
        // 도움 카드가 늘 왼쪽에 있지 않게 섞는다.
        for (let i = offer.length - 1; i > 0; i--) {
            const j = Math.floor(roll() * (i + 1));
            [offer[i], offer[j]] = [offer[j], offer[i]];
        }
        return offer;
    });
}

// 고른 카드가 그날 그 지점에 나온 카드인지 보고 점수를 셈한다. 맞지 않으면 null.
function scoreOf(date, tierId, cards) {
    if (!Array.isArray(cards) || cards.length !== MILESTONES.length) return null;
    const offers = offersFor(date, tierId);
    let score = BASE_SCORE;
    for (let i = 0; i < cards.length; i++) {
        if (!offers[i].includes(cards[i])) return null;
        score += CARDS[cards[i]].points;
    }
    return score;
}
const TIER_IDS = new Set(TIERS.map((tier) => tier.id));

function dateOf(now) {
    return new Date(now + UTC_OFFSET_HOURS * 3600 * 1000).toISOString().slice(0, 10);
}

// 다음 날이 시작될 때까지 남은 초.
function secondsLeft(now) {
    const shifted = now + UTC_OFFSET_HOURS * 3600 * 1000;
    const day = 24 * 3600 * 1000;
    return Math.ceil((day - (shifted % day)) / 1000);
}

// 글자를 수로 바꾼다(FNV-1a). 같은 글은 언제나 같은 수가 된다.
function hash(text) {
    let h = 0x811c9dc5;
    for (const ch of text) {
        h ^= ch.codePointAt(0);
        h = Math.imul(h, 0x01000193) >>> 0;
    }
    return h;
}

// 메인 레벨 칸의 그날 레벨. 어제 뽑힌 자리와 같으면 하나 옆으로 비킨다. 이틀 연속
// 같은 레벨이 나오는 일을 줄이려는 것이고, 완전히 막지는 못한다(어제가 비켜 간 경우).
function levelFor(levels, date) {
    const index = hash(date) % levels.length;
    const yesterday = new Date(Date.parse(date + "T00:00:00Z") - 24 * 3600 * 1000)
        .toISOString().slice(0, 10);
    if (levels.length > 1 && hash(yesterday) % levels.length === index) {
        return levels[(index + 1) % levels.length];
    }
    return levels[index];
}

// 이름에서 제어 문자를 빼고 길이를 자른다. 한글도 한 글자로 센다.
function cleanName(name) {
    if (typeof name !== "string") return null;
    const clean = Array.from(name.replace(/[\u0000-\u001f\u007f]/g, "").trim()).slice(0, MAX_NAME).join("");
    return clean.length > 0 ? clean : null;
}

function validateLevels(levels) {
    if (!Array.isArray(levels) || levels.length === 0) {
        throw new Error("levels.json must be a non-empty array");
    }
    for (const level of levels) {
        if (!Number.isInteger(level.id) || level.id <= 0) throw new Error(`bad level id: ${JSON.stringify(level)}`);
        if (level.kind !== "main" && level.kind !== "online") throw new Error(`bad level kind: ${JSON.stringify(level)}`);
        if (typeof level.name !== "string" || level.name.length === 0) throw new Error(`bad level name: ${JSON.stringify(level)}`);
    }
    return levels;
}

// GD 서버의 레벨 검색 답을 읽는다. 형식은 "레벨들#만든이들#노래들#쪽정보#해시" 이고,
// 레벨들은 | 로, 레벨 하나는 "키:값:키:값..." 으로 나뉜다. 실패하면 "-1" 이 온다.
// (https://github.com/Rifct/gd-docs 의 getGJLevels21 문서를 따랐다.)
function parseSearch(text) {
    if (typeof text !== "string" || text.trim() === "-1" || !text.includes("#")) return null;
    const parts = text.split("#");
    const levels = parts[0].split("|").filter(Boolean).map((raw) => {
        const fields = raw.split(":");
        const level = {};
        for (let i = 0; i + 1 < fields.length; i += 2) level[fields[i]] = fields[i + 1];
        return level;
    });
    const [total] = (parts[3] || "").split(":").map(Number);
    return { levels, total: Number.isFinite(total) ? total : 0 };
}

// GD 서버에 레벨 검색을 보낸다. GD 서버는 User-Agent 가 붙은 요청을 막는 것으로
// 알려져 있어(위 문서의 예제가 그렇게 보낸다) 붙이지 않는다. node 의 https 는
// 스스로 붙이지 않는다.
function searchGD(params) {
    const body = new URLSearchParams({ secret: "Wmfd2893gb7", gameVersion: "22", binaryVersion: "42", ...params }).toString();
    return new Promise((resolve, reject) => {
        const req = https.request("https://www.boomlings.com/database/getGJLevels21.php", {
            method: "POST",
            headers: {
                "Content-Type": "application/x-www-form-urlencoded",
                "Content-Length": Buffer.byteLength(body),
            },
            timeout: 10000,
        }, (res) => {
            const chunks = [];
            res.on("data", (chunk) => chunks.push(chunk));
            res.on("end", () => {
                if (res.statusCode !== 200) return reject(new Error(`GD server answered HTTP ${res.statusCode}`));
                resolve(Buffer.concat(chunks).toString("utf8"));
            });
        });
        req.on("timeout", () => req.destroy(new Error("GD server timed out")));
        req.on("error", reject);
        req.end(body);
    });
}

// 그 난이도의 레이팅된 레벨 하나를 무작위로 고른다. 좋아요 순 목록의 아무 쪽이나
// 열어 그 안에서 하나를 뽑는다. 플랫포머(길이 5)는 시도 수로 겨루기 어려워 뺀다.
async function pickRated(tier, search, random) {
    const filter = {
        type: "2",
        star: "1",
        diff: tier.diff,
        len: "0,1,2,3,4",
        ...(tier.demonFilter ? { demonFilter: tier.demonFilter } : {}),
    };
    const first = parseSearch(await search({ ...filter, page: "0" }));
    if (!first || first.total <= 0) throw new Error(`no rated ${tier.name} levels found`);

    const pages = Math.max(1, Math.min(MAX_PAGES, Math.ceil(first.total / 10)));
    const page = Math.floor(random() * pages);
    const chosen = page === 0 ? first : parseSearch(await search({ ...filter, page: String(page) }));
    const candidates = (chosen?.levels || []).filter((level) =>
        Number(level["1"]) > 0 && level["2"] && Number(level["18"]) > 0 && level["15"] !== "5");
    if (candidates.length === 0) throw new Error(`page ${page} of ${tier.name} had no usable level`);

    const level = candidates[Math.floor(random() * candidates.length)];
    return { id: Number(level["1"]), kind: "online", name: level["2"] };
}

// 기다리다 말 약속. 늦으면 fallback 을 준다. 고르기는 뒤에서 계속되어 다음 요청에 쓰인다.
function within(promise, ms, fallback) {
    let timer;
    const late = new Promise((resolve) => {
        timer = setTimeout(() => resolve(fallback), ms);
    });
    return Promise.race([promise, late]).finally(() => clearTimeout(timer));
}

function createApp({
    levels, now = () => Date.now(), dataFile = null, search = searchGD, random = Math.random,
    // 모드는 20초를 기다린다. 고르기가 그보다 오래 걸려도 답은 먼저 보낸다.
    pickWaitMs = 8000,
} = {}) {
    validateLevels(levels);

    // 날짜 -> { levels: { 난이도 -> 레벨 }, boards: { 난이도 -> { 선수 id -> { name, attempts, at } } } }
    let days = {};
    if (dataFile && fs.existsSync(dataFile)) {
        try {
            days = JSON.parse(fs.readFileSync(dataFile, "utf8")) || {};
        } catch (error) {
            console.error("could not read", dataFile, error.message);
        }
    }

    // 기록이 들어올 때마다 바로 쓰지 않고 잠깐 모았다가 쓴다. 파일은 통째로 새로
    // 쓰고 이름을 바꿔 끼우므로, 쓰는 도중 꺼져도 반쯤 쓴 파일이 남지 않는다.
    let saveTimer = null;
    function save() {
        if (!dataFile || saveTimer) return;
        saveTimer = setTimeout(() => {
            saveTimer = null;
            const temp = dataFile + ".tmp";
            fs.writeFile(temp, JSON.stringify(days), (error) => {
                if (error) return console.error("could not save", error.message);
                fs.rename(temp, dataFile, (renameError) => {
                    if (renameError) console.error("could not save", renameError.message);
                });
            });
        }, 1000);
        saveTimer.unref?.();
    }

    function day(date) {
        const entry = days[date];
        // 예전 형식(날짜 -> 선수들)이 남아 있으면 버리고 새로 시작한다.
        if (!entry || !entry.boards || !entry.levels) {
            days[date] = { levels: {}, boards: {} };
            const dates = Object.keys(days).sort();
            while (dates.length > KEEP_DAYS) delete days[dates.shift()];
        }
        return days[date];
    }

    // 고르는 중인 것과 실패한 것. 같은 난이도를 동시에 여러 번 고르지 않고,
    // 실패하면 한동안은 다시 두드리지 않는다.
    const picking = new Map();
    const failedAt = new Map();

    async function levelOf(date, tier) {
        const today = day(date);
        if (today.levels[tier.id]) return today.levels[tier.id];
        if (tier.id === "main") {
            today.levels.main = levelFor(levels, date);
            save();
            return today.levels.main;
        }

        const key = `${date}/${tier.id}`;
        if (picking.has(key)) return picking.get(key);
        const failed = failedAt.get(key);
        if (failed && now() - failed < RETRY_AFTER_MS) return null;

        const job = pickRated(tier, search, random)
            .then((level) => {
                day(date).levels[tier.id] = level;
                save();
                return level;
            })
            .catch((error) => {
                console.error(`could not pick a ${tier.name} level:`, error.message);
                failedAt.set(key, now());
                return null;
            })
            .finally(() => picking.delete(key));
        picking.set(key, job);
        return job;
    }

    // 점수가 높은 순, 같으면 적은 시도 순, 그것도 같으면 먼저 깬 순.
    function better(a, b) {
        if ((a.score ?? 0) !== (b.score ?? 0)) return (b.score ?? 0) - (a.score ?? 0);
        return a.attempts - b.attempts || a.at - b.at;
    }

    function board(date, tierId, playerId) {
        const players = days[date]?.boards?.[tierId] || {};
        const rows = Object.entries(players)
            .map(([id, row]) => ({ id, ...row }))
            .sort(better);
        const shown = ({ name, attempts, score }) => (score === undefined ? { name, attempts } : { name, attempts, score });
        const result = {
            top: rows.slice(0, TOP).map(shown),
            total: rows.length,
        };
        if (playerId) {
            const index = rows.findIndex((row) => row.id === playerId);
            if (index >= 0) {
                const { attempts, score } = rows[index];
                result.me = { rank: index + 1, attempts, ...(score === undefined ? {} : { score }) };
            }
        }
        return result;
    }

    const posts = new Map();
    function allowPost(address) {
        const minute = Math.floor(now() / 60000);
        const entry = posts.get(address);
        if (!entry || entry.minute !== minute) {
            posts.set(address, { minute, count: 1 });
            if (posts.size > 10000) posts.clear();
            return true;
        }
        entry.count += 1;
        return entry.count <= POSTS_PER_MINUTE;
    }

    // 오늘의 도전. 모든 난이도의 레벨(골라진 것만)과 고른 난이도의 순위표를 함께 준다.
    // 모든 난이도의 레벨을 알아야 모드가 어느 레벨을 열든 오늘의 도전인지 알아본다.
    async function today(tierId = "main", playerId = null) {
        const time = now();
        const date = dateOf(time);
        const tier = TIERS.find((t) => t.id === tierId) || TIERS[0];
        const picked = await Promise.all(TIERS.map((t) => within(levelOf(date, t), pickWaitMs, null)));
        return {
            date,
            secondsLeft: secondsLeft(time),
            tiers: TIERS.map((t, i) => ({ id: t.id, name: t.name, level: picked[i], offers: offersFor(date, t.id) })),
            tier: tier.id,
            level: picked[TIERS.indexOf(tier)],
            roguelike: { milestones: MILESTONES, base: BASE_SCORE, cards: CARDS },
            ...board(date, tier.id, playerId),
        };
    }

    function submit(body, address) {
        const date = dateOf(now());

        if (!body || typeof body !== "object") return [400, { error: "bad-body" }];
        if (body.date !== date) return [409, { error: "day-over", date }];
        if (!TIER_IDS.has(body.tier)) return [400, { error: "bad-tier" }];
        const level = days[date]?.levels?.[body.tier];
        if (!level || body.levelId !== level.id) return [409, { error: "wrong-level", level: level || null }];
        if (!body.player || typeof body.player.id !== "string" || !PLAYER_ID.test(body.player.id)) {
            return [400, { error: "bad-player" }];
        }
        const name = cleanName(body.player.name);
        if (!name) return [400, { error: "bad-name" }];
        if (!Number.isInteger(body.attempts) || body.attempts < 1 || body.attempts > MAX_ATTEMPTS) {
            return [400, { error: "bad-attempts" }];
        }
        const score = scoreOf(date, body.tier, body.cards);
        if (score === null) return [400, { error: "bad-cards" }];
        if (!allowPost(address)) return [429, { error: "slow-down" }];

        const players = (day(date).boards[body.tier] ||= {});
        const previous = players[body.player.id];
        const row = { name, attempts: body.attempts, at: now(), score };
        if (!previous || better(row, previous) < 0) {
            players[body.player.id] = row;
        }
        else {
            previous.name = name;
        }
        save();
        return [200, { date, tier: body.tier, level, ...board(date, body.tier, body.player.id) }];
    }

    function send(res, status, body) {
        const text = JSON.stringify(body);
        res.writeHead(status, {
            "Content-Type": "application/json; charset=utf-8",
            "Content-Length": Buffer.byteLength(text),
            "Cache-Control": "no-store",
        });
        res.end(text);
    }

    const server = http.createServer((req, res) => {
        const url = new URL(req.url, "http://localhost");
        const address = req.socket.remoteAddress || "?";

        if (req.method === "GET" && url.pathname === "/health") {
            return send(res, 200, { ok: true });
        }
        if (req.method === "GET" && url.pathname === "/today") {
            const player = url.searchParams.get("player");
            today(url.searchParams.get("tier") || "main", player && PLAYER_ID.test(player) ? player : null)
                .then((reply) => send(res, 200, reply))
                .catch((error) => {
                    console.error("today failed:", error);
                    send(res, 500, { error: "server-error" });
                });
            return;
        }
        if (req.method === "POST" && url.pathname === "/result") {
            let size = 0;
            const chunks = [];
            let tooBig = false;
            req.on("data", (chunk) => {
                size += chunk.length;
                if (size > MAX_BODY) {
                    tooBig = true;
                    req.destroy();
                    return;
                }
                chunks.push(chunk);
            });
            req.on("end", () => {
                if (tooBig) return;
                let body;
                try {
                    body = JSON.parse(Buffer.concat(chunks).toString("utf8"));
                } catch {
                    return send(res, 400, { error: "bad-json" });
                }
                const [status, reply] = submit(body, address);
                send(res, status, reply);
            });
            return;
        }
        send(res, 404, { error: "not-found" });
    });

    return { server, today, submit };
}

module.exports = {
    createApp, dateOf, levelFor, cleanName, secondsLeft, parseSearch, pickRated, offersFor, scoreOf,
    TIERS, CARDS, MILESTONES,
};

if (require.main === module) {
    // 호스팅 업체는 포트를 자기가 정해서 알려준다. 알려주지 않으면(내 PC 에서 직접
    // 실행) 8788 을 쓴다. 협동 편집기 서버(8787)와 같은 기기에서 함께 돌릴 수 있게.
    const PORT = process.env.PORT || 8788;
    const levels = JSON.parse(fs.readFileSync(path.join(__dirname, "levels.json"), "utf8"));
    const dataFile = process.env.DATA_FILE || path.join(__dirname, "data.json");
    const { server } = createApp({ levels, dataFile });
    server.listen(PORT, () => console.log(`daily challenge server on port ${PORT}`));
}
