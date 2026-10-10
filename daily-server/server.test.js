// node --test 로 돈다. 바깥 패키지 없이 node 만 있으면 된다.
const test = require("node:test");
const assert = require("node:assert");
const { createApp, dateOf, levelFor, cleanName, secondsLeft } = require("./server");
const levels = require("./levels.json");

// 2026-10-10 12:00 한국 시간
const NOON_KST = Date.parse("2026-10-10T03:00:00Z");

function app(time = NOON_KST) {
    let clock = time;
    const made = createApp({ levels, now: () => clock });
    made.setClock = (value) => { clock = value; };
    return made;
}

function result(made, overrides = {}) {
    const today = made.today();
    return made.submit({
        date: today.date,
        levelId: today.level.id,
        player: { id: "acc:1", name: "doong" },
        attempts: 10,
        ...overrides,
    }, "1.2.3.4");
}

test("the day turns at midnight Korean time", () => {
    assert.strictEqual(dateOf(Date.parse("2026-10-10T14:59:59Z")), "2026-10-10");
    assert.strictEqual(dateOf(Date.parse("2026-10-10T15:00:00Z")), "2026-10-11");
    assert.strictEqual(secondsLeft(Date.parse("2026-10-10T14:59:00Z")), 60);
});

test("the same date always gives the same level, and the pool is valid", () => {
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

test("fewest attempts ranks first, ties go to whoever was first", () => {
    const made = app();
    result(made, { player: { id: "acc:1", name: "a" }, attempts: 30 });
    made.setClock(NOON_KST + 1000);
    result(made, { player: { id: "acc:2", name: "b" }, attempts: 12 });
    made.setClock(NOON_KST + 2000);
    const [status, reply] = result(made, { player: { id: "acc:3", name: "c" }, attempts: 12 });
    assert.strictEqual(status, 200);
    assert.deepStrictEqual(reply.top.map((row) => row.name), ["b", "c", "a"]);
    assert.deepStrictEqual(reply.me, { rank: 2, attempts: 12 });
    assert.strictEqual(reply.total, 3);
});

test("a player keeps only their best run of the day", () => {
    const made = app();
    result(made, { attempts: 20 });
    result(made, { attempts: 50 });
    let [, reply] = result(made, { attempts: 15 });
    assert.deepStrictEqual(reply.me, { rank: 1, attempts: 15 });
    assert.strictEqual(reply.total, 1);
    [, reply] = result(made, { attempts: 99, player: { id: "acc:1", name: "renamed" } });
    assert.deepStrictEqual(reply.top, [{ name: "renamed", attempts: 15 }]);
});

test("wrong day, wrong level and bad input are refused", () => {
    const made = app();
    assert.strictEqual(result(made, { date: "2026-10-09" })[0], 409);
    assert.strictEqual(result(made, { levelId: -1 })[0], 409);
    assert.strictEqual(result(made, { attempts: 0 })[0], 400);
    assert.strictEqual(result(made, { attempts: 1.5 })[0], 400);
    assert.strictEqual(result(made, { player: { id: "nope", name: "x" } })[0], 400);
    assert.strictEqual(result(made, { player: { id: "acc:1", name: "\u0001  " } })[0], 400);
    assert.strictEqual(made.submit(null, "x")[0], 400);
});

test("a new day starts with an empty board", () => {
    const made = app();
    result(made);
    made.setClock(NOON_KST + 24 * 3600 * 1000);
    assert.strictEqual(made.today().total, 0);
});

test("one address cannot flood the board", () => {
    const made = app();
    let last;
    for (let i = 0; i < 31; i++) last = result(made, { player: { id: `acc:${i}`, name: "x" } });
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
        const today = await (await fetch(`${base}/today`)).json();
        assert.ok(today.level && today.date);

        const posted = await fetch(`${base}/result`, {
            method: "POST",
            body: JSON.stringify({
                date: today.date, levelId: today.level.id,
                player: { id: "dev:abc123", name: "tester" }, attempts: 7,
            }),
        });
        assert.strictEqual(posted.status, 200);

        const mine = await (await fetch(`${base}/today?player=dev:abc123`)).json();
        assert.deepStrictEqual(mine.me, { rank: 1, attempts: 7 });

        assert.strictEqual((await fetch(`${base}/result`, { method: "POST", body: "{" })).status, 400);
        assert.strictEqual((await fetch(`${base}/nope`)).status, 404);
    }
    finally {
        made.server.close();
    }
});
