#!/usr/bin/env python3
"""번역이 끝난 척하는 문장을 찾는다.

덮었다고 세는 것만으로는 모자라다. 엉뚱한 틀에 걸려 나온 한국어도 덮인 것으로
세어지기 때문이다. 그래서 4,872개를 전부 실제로 그려 보고, 한국어 한가운데에
영어가 그대로 서 있는 것을 골라낸다. 편집기 칸 이름처럼 영어로 두어야 하는 말은
아래 목록으로 넘긴다.
"""

import importlib.util
import json
import pathlib
import re

HERE = pathlib.Path(__file__).parent
spec = importlib.util.spec_from_file_location("render", HERE / "render.py")
render = importlib.util.module_from_spec(spec)
spec.loader.exec_module(render)

WORD = re.compile(r"\b[A-Za-z]{3,}\b")
TAG = re.compile(r"<[^>]*>")

# 화면에 영어로 적혀 있는 칸 이름과 고유명사. 번역하면 오히려 못 찾는다.
KEEP = set(
    "geometry dash robtop newgrounds geode qolmod fps tps png api url json apx hsv "
    "ldm cbf gddl aredl pointercrate pemonlist argon dashauth spotify discord luma "
    "allium jukebox accept com creative commons games itemid sfxgroup fadein fadeout "
    "maintime groupid spawngid targetpos refchannel offx offy dualdir uselum wavew "
    "timeoff maxsize prox curve even dist mode ord extra preview playback reversed "
    "lock instant anim shine speed animate only frame offset single points particle "
    "toggle trigger easing target direction dynamic small step aim follow move time "
    "next free copy and paste counter attempts left right align blending vertex the "
    "www audio listen group center ccw close ref unique start end ignore volume rgb "
    "spawn hold dual haxxor mvp rtx fmod ctrl scrollwheel rubrub robtroll robert "
    "bpm ufo lol fnf blame toe2 ncs sfx http zip mac ios sui".split()
)


def achievements(exact, patterns) -> int:
    """업적은 게임의 문자열 목록이 아니라 따로 있는 표에서 나온다. 그래서 4,872개를
    다 덮어도 업적은 영어로 남을 수 있다. 이름과 설명을 모두 본다.

    표는 스팀이 들고 있는 게임의 업적 목록에서 왔다. 게임이 말하는 546개와 수가
    맞으므로, 게임 밖에서 구할 수 있는 것 가운데 이것이 진짜다."""
    data = json.loads((HERE / "achievements.json").read_text(encoding="utf-8"))
    missing = []
    for item in data:
        for field in ("title", "name", "description", "achievedDescription"):
            text = item.get(field)
            if text and render.render(text, exact, patterns) is None:
                missing.append((item["id"], field, text))
    print(f"{len(missing)} achievement strings untranslated")
    for ident, field, text in missing:
        print(f"  {ident} {field}: {text!r}")
    return len(missing)


def keep_english(path) -> set:
    import json as _json
    table = _json.loads(path.read_text(encoding="utf-8"))
    return set(table.get("names", []))


# 윈도우 헤더가 오래전부터 차지하고 있는 이름들. 이 이름으로 상수를 두면
# 안드로이드와 맥에서는 멀쩡히 빌드되다가 윈도우에서만 깨진다. 한 바퀴를
# 통째로 버리게 되므로 여기서 미리 잡는다.
WINDOWS_MACROS = (
    "NEAR FAR IN OUT ERROR DELETE MIN MAX INFINITE OPTIONAL CONST VOID "
    "TRUE FALSE interface small near far PASCAL WINAPI CALLBACK"
).split()

DECLARE = re.compile(
    r"\b(?:constexpr|const|static|enum|#\s*define)\b[^;\n]*?\b(" +
    "|".join(WINDOWS_MACROS) + r")\b\s*(?:=|\{|\()"
)


def windows_macro_clashes() -> int:
    hits = 0
    for path in sorted((HERE.parent / "src").rglob("*.?pp")):
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if line.lstrip().startswith("//"):
                continue
            found = DECLARE.search(line)
            if found:
                print(f"  {path.name}:{number} 은(는) 윈도우 매크로 이름 "
                      f"{found.group(1)} 을(를) 쓴다")
                hits += 1
    print(f"{hits} windows macro clashes")
    return hits


def main() -> None:
    exact, patterns = render.cov.load()
    # 그대로 두기로 한 이름은 문장 안에 남아 있어도 빠진 것이 아니다.
    names = keep_english(HERE.parent / "translations" / "ko.json")
    for name in names:
        for word in TAG.sub("", name).split():
            KEEP.add(word.lower())
    windows_macro_clashes()
    achievements(exact, patterns)
    keys = json.loads((HERE / "gd-strings.json").read_text(encoding="utf-8")).keys()
    suspect = []
    for key in keys:
        out = render.render(key, exact, patterns)
        if not out or not any("가" <= c <= "힣" for c in out):
            continue
        left = [w for w in WORD.findall(TAG.sub("", out)) if w.lower() not in KEEP]
        if not left:
            continue
        # 긴 글은 편집기의 칸 이름을 그대로 가리키느라 영어가 섞이는 것이
        # 맞다. 그래서 셋 이상 남았을 때만 의심한다.
        #
        # 짧은 글은 다르다. 이름 한 덩어리가 통째로 화면에 뜨는 자리이므로
        # 낱말 하나만 영어로 남아도 "영혼 조각" 이 "Soul 조각" 이 된다.
        # 따옴표로 묶인 것은 레벨 이름이니 그대로 두는 것이 맞다.
        short = len(TAG.sub("", key).split()) <= 5 and "'" not in out
        if len(left) >= 3 or short:
            suspect.append((key, out, left[:6]))
    print(f"{len(suspect)} suspect")
    for key, out, left in suspect:
        print(f"  {left}\n    {key[:70]!r}\n    -> {out[:100]!r}")


if __name__ == "__main__":
    main()
