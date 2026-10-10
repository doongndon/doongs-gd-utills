// Doong's GD Utils 오늘의 도전 서버
//
// 하는 일
// - 날마다 레벨 하나를 고른다. 날짜만 같으면 누가 물어도 같은 레벨이 나온다.
// - 그 레벨을 깬 사람의 시도 수를 받아 적은 순으로 순위를 매긴다.
// - 한 사람은 하루에 한 줄. 더 적은 시도로 다시 깨면 그 기록으로 바꾼다.
//
// 기록은 정직함에 기댄다. 모드가 연습 모드나 시작 위치로 깬 것은 보내지 않지만,
// 서버는 누가 보냈는지 확인할 방법이 없다. 상금을 거는 대회가 아니라 재미로
// 견주는 판이다.
//
// 바깥 패키지는 쓰지 않는다. node 만 있으면 돈다.
const http = require("http");
const fs = require("fs");
const path = require("path");

// 하루가 바뀌는 시각. 쓰는 사람이 대부분 한국에 있으므로 한국 시간 자정이 기본이다.
const UTC_OFFSET_HOURS = Number(process.env.UTC_OFFSET_HOURS ?? 9);
const TOP = 10;
const MAX_NAME = 20;
const MAX_ATTEMPTS = 1000000;
const MAX_BODY = 2048;
// 지난 날의 순위는 이만큼만 들고 있는다. 오래 켜 둬도 메모리가 늘지 않게.
const KEEP_DAYS = 7;
// 한 주소에서 1분에 보낼 수 있는 기록 수. 실수든 장난이든 서버를 못 쓰게 만들지 않게.
const POSTS_PER_MINUTE = 30;
const PLAYER_ID = /^(acc|dev):[A-Za-z0-9]{1,32}$/;

function dateOf(now) {
    return new Date(now + UTC_OFFSET_HOURS * 3600 * 1000).toISOString().slice(0, 10);
}

// 다음 날이 시작될 때까지 남은 초.
function secondsLeft(now) {
    const shifted = now + UTC_OFFSET_HOURS * 3600 * 1000;
    const day = 24 * 3600 * 1000;
    return Math.ceil((day - (shifted % day)) / 1000);
}

// 날짜 글자를 수로 바꾼다(FNV-1a). 같은 날짜는 언제나 같은 수가 된다.
function hash(text) {
    let h = 0x811c9dc5;
    for (const ch of text) {
        h ^= ch.codePointAt(0);
        h = Math.imul(h, 0x01000193) >>> 0;
    }
    return h;
}

// 그날의 레벨. 어제 뽑힌 자리와 같으면 하나 옆으로 비킨다. 이틀 연속 같은 레벨이
// 나오는 일을 줄이려는 것이고, 완전히 막지는 못한다(어제가 비켜 간 경우).
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

function createApp({ levels, now = () => Date.now(), dataFile = null } = {}) {
    validateLevels(levels);

    // 날짜 -> { 선수 id -> { name, attempts, at } }
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

    function forget(today) {
        const dates = Object.keys(days).sort();
        while (dates.length > KEEP_DAYS) {
            const old = dates.shift();
            if (old !== today) delete days[old];
        }
    }

    function board(date, playerId) {
        const players = days[date] || {};
        const rows = Object.entries(players)
            .map(([id, row]) => ({ id, ...row }))
            .sort((a, b) => a.attempts - b.attempts || a.at - b.at);
        const result = {
            top: rows.slice(0, TOP).map(({ name, attempts }) => ({ name, attempts })),
            total: rows.length,
        };
        if (playerId) {
            const index = rows.findIndex((row) => row.id === playerId);
            if (index >= 0) result.me = { rank: index + 1, attempts: rows[index].attempts };
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

    function send(res, status, body) {
        const text = JSON.stringify(body);
        res.writeHead(status, {
            "Content-Type": "application/json; charset=utf-8",
            "Content-Length": Buffer.byteLength(text),
            "Cache-Control": "no-store",
        });
        res.end(text);
    }

    function today(playerId) {
        const time = now();
        const date = dateOf(time);
        return {
            date,
            level: levelFor(levels, date),
            secondsLeft: secondsLeft(time),
            ...board(date, playerId),
        };
    }

    function submit(body, address) {
        const date = dateOf(now());
        const level = levelFor(levels, date);

        if (!body || typeof body !== "object") return [400, { error: "bad-body" }];
        if (body.date !== date) return [409, { error: "day-over", date }];
        if (body.levelId !== level.id) return [409, { error: "wrong-level", level }];
        if (!body.player || typeof body.player.id !== "string" || !PLAYER_ID.test(body.player.id)) {
            return [400, { error: "bad-player" }];
        }
        const name = cleanName(body.player.name);
        if (!name) return [400, { error: "bad-name" }];
        if (!Number.isInteger(body.attempts) || body.attempts < 1 || body.attempts > MAX_ATTEMPTS) {
            return [400, { error: "bad-attempts" }];
        }
        if (!allowPost(address)) return [429, { error: "slow-down" }];

        const players = (days[date] ||= {});
        const previous = players[body.player.id];
        if (!previous || body.attempts < previous.attempts) {
            players[body.player.id] = { name, attempts: body.attempts, at: now() };
        }
        else {
            previous.name = name;
        }
        forget(date);
        save();
        return [200, { date, level, ...board(date, body.player.id) }];
    }

    const server = http.createServer((req, res) => {
        const url = new URL(req.url, "http://localhost");
        const address = req.socket.remoteAddress || "?";

        if (req.method === "GET" && url.pathname === "/health") {
            return send(res, 200, { ok: true });
        }
        if (req.method === "GET" && url.pathname === "/today") {
            const player = url.searchParams.get("player");
            return send(res, 200, today(player && PLAYER_ID.test(player) ? player : null));
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

module.exports = { createApp, dateOf, levelFor, cleanName, secondsLeft };

if (require.main === module) {
    // 호스팅 업체는 포트를 자기가 정해서 알려준다. 알려주지 않으면(내 PC 에서 직접
    // 실행) 8788 을 쓴다. 협동 편집기 서버(8787)와 같은 기기에서 함께 돌릴 수 있게.
    const PORT = process.env.PORT || 8788;
    const levels = JSON.parse(fs.readFileSync(path.join(__dirname, "levels.json"), "utf8"));
    const dataFile = process.env.DATA_FILE || path.join(__dirname, "data.json");
    const { server } = createApp({ levels, dataFile });
    server.listen(PORT, () => console.log(`daily challenge server on port ${PORT}`));
}
