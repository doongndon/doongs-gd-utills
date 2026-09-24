#!/usr/bin/env python3
"""모드 소스에서 화면에 나오는 영어 글자를 뽑는다.

라벨과 단추와 팝업을 만드는 함수에 넘긴 문자열만 본다. 파일 이름이나 노드 ID
같은 것이 섞이면 표만 지저분해지므로, 부르는 함수 이름으로 걸러낸다.
"""

import json
import pathlib
import re
import sys

# 글자가 화면에 올라가는 자리들. 첫 번째 인자가 글이 아닌 것은 위치를 적어 둔다.
CALLS = {
    "CCLabelBMFont::create": 0,
    "ButtonSprite::create": 0,
    "FLAlertLayer::create": None,     # 여러 인자가 모두 글이다
    "createQuickPopup": None,
    "SimpleTextArea::create": 0,
    "MDTextArea::create": 0,
    "TextArea::create": 0,
    "Notification::create": 0,
    "TextInput::create": 1,
    "setTitle": 0,
    "setString": 0,
    "setPlaceholder": 0,
    "setPrompt": 0,
    "setBtnText": 0,
    "MDPopup::create": None,
    "fmt::format": 0,
    "setDesc": 0,
    "setSubtitle": 0,
    "addButton": 0,
}

STRING = re.compile(r'"((?:[^"\\]|\\.)*)"')
CALL = re.compile(r"\b(" + "|".join(re.escape(k) for k in CALLS) + r")\s*\(")

# 글로 보기 어려운 것들
SKIP = re.compile(
    r"^\s*$"
    r"|^[\W\d_]+$"                       # 기호와 숫자뿐
    r"|\.(png|fnt|plist|json|mp3|ogg|txt|md|geode|so|dll|dylib)$"
    r"|^[a-z0-9]+([-_][a-z0-9]+)+$"      # node-id 꼴
    r"|^[a-z]+\.[a-z0-9._-]+$"           # mod.id 꼴
    r"|^https?://"
    r"|^%[sdif]$"
)


def looks_like_text(s: str) -> bool:
    if len(s) < 2 or len(s) > 300:
        return False
    if SKIP.search(s):
        return False
    # 알파벳이 두 글자 이상 이어지는 곳이 있어야 말이다
    return bool(re.search(r"[A-Za-z]{2}", s))


def unescape(s: str) -> str:
    return (s.replace('\\n', '\n').replace('\\t', '\t')
             .replace('\\"', '"').replace('\\\\', '\\'))


def strings_in_call(line: str, start: int) -> list[str]:
    """여는 괄호부터 짝이 맞는 닫는 괄호까지의 문자열들."""
    depth = 0
    out = []
    i = start
    while i < len(line):
        c = line[i]
        if c == '"':
            joined = ""
            while i < len(line):
                m = STRING.match(line, i)
                if not m:
                    break
                joined += m.group(1)
                i = m.end()
                nxt = re.match(r'[\s\\]*', line[i:])
                after = i + nxt.end()
                if after < len(line) and line[after] == '"':
                    i = after
                    continue
                break
            if joined:
                out.append(joined)
            continue
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                break
        i += 1
    return out


def harvest(root: pathlib.Path) -> set[str]:
    found: set[str] = set()
    for path in root.rglob("*"):
        if path.suffix not in (".cpp", ".hpp", ".h", ".cc"):
            continue
        try:
            text = path.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        for m in CALL.finditer(text):
            for s in strings_in_call(text, m.end() - 1):
                s = unescape(s)
                if looks_like_text(s):
                    found.add(s)

    # mod.json 의 설정 이름과 설명도 화면에 그대로 나온다
    for path in root.rglob("mod.json"):
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except Exception:
            continue
        for key, value in (data.get("settings") or {}).items():
            if not isinstance(value, dict):
                continue
            for field in ("name", "description"):
                s = value.get(field)
                if isinstance(s, str) and looks_like_text(s):
                    found.add(s)
    return found


def main() -> None:
    root = pathlib.Path(sys.argv[1])
    found = harvest(root)
    out = pathlib.Path(sys.argv[2])
    out.write_text(json.dumps(sorted(found), indent=1, ensure_ascii=False), encoding="utf-8")
    print(f"{len(found)} strings -> {out}")


if __name__ == "__main__":
    main()
