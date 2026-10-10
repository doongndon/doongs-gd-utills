// node --test 로 돈다. 바깥 패키지 없이 node 만 있으면 된다.
// GD 서버 대신 가짜 검색을 넣어 시험한다. 답의 모양은 gd-docs 의 getGJLevels21 예시를 따른다.
const test = require("node:test");
const assert = require("node:assert");
const { createApp, dateOf, levelFor, cleanName, secondsLeft, parseSearch, pickRated, TIERS } = require("./server");
const levels = require("./levels.json");

// 2026-10-10 12:00 한국 시간
const NOON_KST = Date.parse("2026-10-10T03:00:00Z");

// 레벨 하나를 GD 서버가 주는 "키:값" 줄로 만든다. 1=ID, 2=이름, 15=길이, 18=별
function gdLevel(id, name, { length = 2, stars = 4 } = {}) {
    return `1:${id}:2:${name}:5:1:6:100:8:10:9:30:10:5000:12:0:13:21:14:100:17::43:0:25::18:${stars}:19:0:42:0:45:0:3::15:${length}:30:0:31:0`;
}

// 난이도마다 레벨 25개가 있는 가짜 GD 서버. 쪽마다 10개씩 준다.
function fakeGD({ failFor = new Set(), calls = [] } = {}) {
    return async (params) => {
        calls.push(params);
        if (failFor.has(params.diff)) return "-1";
        const total = 25;
        const page = Number(params.page);
        const rows = [];
        for (let i = page * 10; i < Math.min(total, page * 10 + 10); i++) {
            rows.push(gdLevel(1000 * Number(params.diff.replace("-", "9")) + i, `Level ${params.diff}/${i}`));
        }
        return `${rows.join("|")}#100:Creator:200#songs#${total}:${page * 10}:10#hash`;
    };
}

function app({ time = NOON_KST, ...options } = {}) {
    let clock = time;
    let roll = 0;
    const made = createApp({
        levels,
        now: () => clock,
        search: fakeGD(options),
        // 차례로 돌아가는 가짜 주사위. 늘 같은 결과가 나와 시험이 흔들리지 않는다.
        random: () => (roll = (roll + 0.37) % 1),
        pickWaitMs: options.pickWaitMs ?? 1000,
        ...(options.search ? { search: options.search } : {}),
    });
    made.setClock = (value) => { clock = value; };
    return made;
}

async function result(made, overrides = {}) {
    const tier = overrides.tier || "main";
    const today = await made.today(tier);
    return made.submit({
        date: today.date,
        tier,
        levelId: today.level.id,
        player: { id: "acc:1", name: "doong" },
        attempts: 10,
        ...overrides,
    }, overrides.address || "1.2.3.4");
}

test("the day turns at midnight Korean time", () => {
    assert.strictEqual(dateOf(Date.parse("2026-10-10T14:59:59Z")), "2026-10-10");
    assert.strictEqual(dateOf(Date.parse("2026-10-10T15:00:00Z")), "2026-10-11");
    assert.strictEqual(secondsLeft(Date.parse("2026-10-10T14:59:00Z")), 60);
});

test("the main-level pool gives the same level all day and rarely repeats", () => {
    assert.deepStrictEqual(levelFor(levels, "2026-10-10"), levelFor(levels, "2026-10-10"));
    let repeats = 0;
    let previous = null;
    for (let day = 0; day < 365; day++) {
        const date = new Date(Date.parse("2026-01-01T00:00:00Z") + day * 86400000).toISOString().slice(0, 10);
        const level = levelFor(levels, date);
        assert.ok(levels.includes(level));
        if (previous && previous.id === level.id) repeats++;
        previous = level;
    }
    assert.ok(repeats < 10, `too many back-to-back repeats: ${repeats}`);
});

test("a GD search answer is read the way the docs describe it", () => {
    const docsExample = "1:10565740:2:Bloodbath:5:3:6:503085:8:10:9:50:10:44138442:12:0:13:21:14:2375318:17:1:43:6:25::18:10:19:10330:42:0:45:24746:3:V2hv:15:3:30:7679228:31:0:37:0:38:0:39:0:46:1:47:2:35:467339#503085:Riot:37415#songs#4:0:10#1664b8bb";
    const parsed = parseSearch(docsExample);
    assert.strictEqual(parsed.total, 4);
    assert.strictEqual(parsed.levels[0]["1"], "10565740");
    assert.strictEqual(parsed.levels[0]["2"], "Bloodbath");
    assert.strictEqual(parsed.levels[0]["18"], "10");
    assert.strictEqual(parseSearch("-1"), null);
    assert.strictEqual(parseSearch(""), null);
});

test("a rated pick asks for rated, non-platformer levels of the tier and skips bad rows", async () => {
    const calls = [];
    const tier = TIERS.find((t) => t.id === "easy-demon");
    const search = async (params) => {
        calls.push(params);
        return [
            gdLevel(1, "Platformer", { length: 5 }),
            gdLevel(2, "Unrated", { stars: 0 }),
            gdLevel(3, "Good"),
        ].join("|") + "#c#s#3:0:10#h";
    };
    const level = await pickRated(tier, search, () => 0);
    assert.deepStrictEqual(level, { id: 3, kind: "online", name: "Good" });
    assert.strictEqual(calls[0].star, "1");
    assert.strictEqual(calls[0].diff, "-2");
    assert.strictEqual(calls[0].demonFilter, "1");
    assert.strictEqual(calls[0].len, "0,1,2,3,4");
});

test("every tier gets a level, and it stays the same all day", async () => {
    const made = app();
    const first = await made.today("hard");
    assert.strictEqual(first.tiers.length, TIERS.length);
    for (const tier of first.tiers) assert.ok(tier.level, `${tier.id} has no level`);
    assert.strictEqual(first.tiers.find((t) => t.id === "main").level.kind, "main");
    assert.strictEqual(first.level.kind, "online");

    made.setClock(NOON_KST + 3600 * 1000);
    const later = await made.today("hard");
    assert.deepStrictEqual(later.tiers, first.tiers);

    made.setClock(NOON_KST + 24 * 3600 * 1000);
    const tomorrow = await made.today("hard");
    assert.notStrictEqual(tomorrow.date, first.date);
});

test("when the GD server fails, the tier is empty and is not asked again straight away", async () => {
    const calls = [];
    const made = app({ failFor: new Set(["3"]), calls });
    const first = await made.today("hard");
    assert.strictEqual(first.level, null);
    assert.ok(first.tiers.find((t) => t.id === "main").level, "main levels still work");
    const asked = calls.filter((c) => c.diff === "3").length;
    await made.today("hard");
    assert.strictEqual(calls.filter((c) => c.diff === "3").length, asked);
});

test("a slow GD server does not hold up the answer", async () => {
    const made = app({
        pickWaitMs: 50,
        search: () => new Promise(() => {}),
    });
    const started = Date.now();
    const reply = await made.today("easy");
    assert.ok(Date.now() - started < 1000);
    assert.strictEqual(reply.level, null);
});

test("each tier has its own board, fewest attempts first", async () => {
    const made = app();
    await result(made, { tier: "easy", player: { id: "acc:1", name: "a" }, attempts: 30 });
    made.setClock(NOON_KST + 1000);
    await result(made, { tier: "easy", player: { id: "acc:2", name: "b" }, attempts: 12 });
    made.setClock(NOON_KST + 2000);
    const [status, reply] = await result(made, { tier: "easy", player: { id: "acc:3", name: "c" }, attempts: 12 });
    assert.strictEqual(status, 200);
    assert.deepStrictEqual(reply.top.map((row) => row.name), ["b", "c", "a"]);
    assert.deepStrictEqual(reply.me, { rank: 2, attempts: 12 });

    const other = await made.today("insane");
    assert.strictEqual(other.total, 0);
});

test("a player keeps only their best run of the day per tier", async () => {
    const made = app();
    await result(made, { attempts: 20 });
    await result(made, { attempts: 50 });
    let [, reply] = await result(made, { attempts: 15 });
    assert.deepStrictEqual(reply.me, { rank: 1, attempts: 15 });
    [, reply] = await result(made, { attempts: 99, player: { id: "acc:1", name: "renamed" } });
    assert.deepStrictEqual(reply.top, [{ name: "renamed", attempts: 15 }]);
});

test("wrong day, wrong tier, wrong level and bad input are refused", async () => {
    const made = app();
    assert.strictEqual((await result(made, { date: "2026-10-09" }))[0], 409);
    assert.strictEqual((await result(made, { levelId: -1 }))[0], 409);
    assert.strictEqual((await result(made, { tier: "nope", levelId: 1 }).catch(() => [400]))[0], 400);
    assert.strictEqual((await result(made, { attempts: 0 }))[0], 400);
    assert.strictEqual((await result(made, { attempts: 1.5 }))[0], 400);
    assert.strictEqual((await result(made, { player: { id: "nope", name: "x" } }))[0], 400);
    assert.strictEqual((await result(made, { player: { id: "acc:1", name: "\u0001  " } }))[0], 400);
    assert.strictEqual(made.submit(null, "x")[0], 400);
});

test("one address cannot flood the board", async () => {
    const made = app();
    let last;
    for (let i = 0; i < 31; i++) last = await result(made, { player: { id: `acc:${i}`, name: "x" } });
    assert.strictEqual(last[0], 429);
});

test("names are cleaned and cut by letters, not bytes", () => {
    assert.strictEqual(cleanName("  doong\n "), "doong");
    assert.strictEqual(cleanName("가".repeat(30)), "가".repeat(20));
    assert.strictEqual(cleanName(42), null);
});

test("the HTTP routes answer", async () => {
    const made = app();
    await new Promise((resolve) => made.server.listen(0, resolve));
    const base = `http://127.0.0.1:${made.server.address().port}`;
    try {
        const today = await (await fetch(`${base}/today?tier=normal`)).json();
        assert.strictEqual(today.tier, "normal");
        assert.ok(today.level && today.date);

        const posted = await fetch(`${base}/result`, {
            method: "POST",
            body: JSON.stringify({
                date: today.date, tier: "normal", levelId: today.level.id,
                player: { id: "dev:abc123", name: "tester" }, attempts: 7,
            }),
        });
        assert.strictEqual(posted.status, 200);

        const mine = await (await fetch(`${base}/today?tier=normal&player=dev:abc123`)).json();
        assert.deepStrictEqual(mine.me, { rank: 1, attempts: 7 });

        const unknownTier = await (await fetch(`${base}/today?tier=zzz`)).json();
        assert.strictEqual(unknownTier.tier, "main");

        assert.strictEqual((await fetch(`${base}/result`, { method: "POST", body: "{" })).status, 400);
        assert.strictEqual((await fetch(`${base}/nope`)).status, 404);
    }
    finally {
        made.server.close();
    }
});
