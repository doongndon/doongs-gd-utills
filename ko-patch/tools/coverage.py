#!/usr/bin/env python3
"""모드의 표가 GD 전체 문자열 중 몇 개를 덮는지 센다. Translator.cpp 와 같은 규칙."""

import json
import re
import pathlib
import sys

KO = pathlib.Path(__file__).parent.parent / "translations" / "ko.json"
ALL = pathlib.Path(__file__).parent / "gd-strings.json"


TAG = re.compile(r"<(/?c[a-z0-9-]*)>")


def too_many_tags(capture):
    """조각 하나가 담아도 되는 색표는 한 벌뿐. Translator.cpp 와 같은 규칙."""
    opens = closes = 0
    for m in TAG.finditer(capture):
        tag = m.group(1)
        if tag == "/c":
            closes += 1
        elif tag[0] == "c" and (len(tag) == 2 or tag[1] == "-"):
            opens += 1
    return closes > 1 or opens != closes


BLANK = re.compile(r"\{(#?)\}")


def split_pattern(source):
    """틀을 빈칸으로 자른다. {#} 은 숫자만 받는 자리. Translator.cpp 와 같다."""
    segments, numeric, start = [], [], 0
    for m in BLANK.finditer(source):
        segments.append(source[start:m.start()])
        numeric.append(bool(m.group(1)))
        start = m.end()
    segments.append(source[start:])
    return (segments, numeric) if len(segments) >= 2 else None


def load():
    data = json.loads(KO.read_text(encoding="utf-8"))
    # 번역하지 않기로 한 이름들. 덮은 것으로 세되 글자는 그대로 둔다.
    for name in data.get("names", []):
        data["exact"][name] = name  # 이름이 표보다 앞선다
    exact = dict(data["exact"])
    for source, korean in data["exact"].items():
        # 줄바꿈이 공백으로 바뀌어 들어오는 자리가 있어 그 꼴도 함께 담는다.
        if "\n" in source:
            exact.setdefault(source.replace("\n", " "), korean)
    patterns = []
    for source, korean in data["patterns"].items():
        cut = split_pattern(source)
        if cut is None:
            continue
        patterns.append(cut + (korean,))
        if "\n" in source:
            flat = split_pattern(source.replace("\n", " "))
            if flat:
                patterns.append(flat + (korean,))
    # 고정 글자가 긴 틀부터
    patterns.sort(key=lambda p: -sum(len(s) for s in p[0]))
    return exact, patterns


def is_plain_ascii(text):
    return all(0x20 <= ord(c) <= 0x7e for c in text)


COUNTERS = ("개", "명", "곡", "번", "쪽", "점", "초", "분", "일", "해", "달",
            "시간", "층", "줄", "칸", "가지", "도", "위")


SPEC = re.compile(r"%[-+ #0-9.]*[a-zA-Z]$")


def is_numeric(text):
    if SPEC.fullmatch(text):
        return True
    digit = False
    for c in text:
        if c.isdigit():
            digit = True
        elif c not in ",.-+ ":
            return False
    return digit


SLOT = re.compile(r"\{([0-9]*)\}")


def numeric_slots(korean):
    """번역문의 빈칸마다 (자리 번호, 세는 자리인가). Translator.cpp 와 같다."""
    slots = []
    nxt = 0
    for m in SLOT.finditer(korean):
        if m.group(1):
            index = int(m.group(1))
        else:
            index = nxt
            nxt += 1
        rest = korean[m.end():]
        if rest.startswith(" "):
            rest = rest[1:]
        slots.append((index, rest.startswith(COUNTERS)))
    return slots


def apply_patterns(text, patterns, origin=None):
    for segments, numeric, korean in patterns:
        if not text.startswith(segments[0]) or not text.endswith(segments[-1]):
            continue
        cursor = len(segments[0])
        captures = []
        ok = True
        for i in range(1, len(segments)):
            seg = segments[i]
            if i + 1 == len(segments):
                at = len(text) - len(seg)
                if at < cursor:
                    ok = False
                    break
            else:
                at = text.find(seg, cursor)
                if at == -1 or not seg:
                    ok = False
                    break
            captures.append(text[cursor:at])
            cursor = at + len(seg)
        if not ok or not captures:
            continue
        if not all(is_plain_ascii(c) for c in captures):
            continue
        # 편 글로 맞춘 것이라면, 조각이 원래 글에서 줄을 넘나들지 않아야 한다.
        # Translator.cpp 와 같은 규칙.
        if origin is not None and any(c not in origin for c in captures):
            continue
        # 값 하나가 제 색을 입고 오는 것은 막지 않는다. 색표가 여러 벌이면
        # 문장을 삼킨 것이다. Translator.cpp 와 같은 규칙.
        if any(too_many_tags(c) for c in captures):
            continue
        if any(n and i < len(captures) and not is_numeric(captures[i])
               for i, n in enumerate(numeric)):
            continue
        if any(wants and index < len(captures) and not is_numeric(captures[index])
               for index, wants in numeric_slots(korean)):
            continue
        return korean
    return None


def lookup(text, exact, patterns, origin=None):
    if text in exact:
        return exact[text]
    return apply_patterns(text, patterns, origin)


def translate(text, exact, patterns):
    hit = lookup(text, exact, patterns)
    if hit is not None:
        return hit
    # 자리가 좁으면 GD 가 줄바꿈을 끼워 넣는다. 펴서 한 번 더 찾아본다.
    # Translator.cpp 와 같은 규칙이다.
    if "\n" not in text:
        return None
    return lookup(text.replace("\n", " "), exact, patterns, text)


def main():
    exact, patterns = load()
    keys = list(json.loads(ALL.read_text(encoding="utf-8")).keys())
    missing = [k for k in keys if translate(k, exact, patterns) is None]
    print(f"{len(keys) - len(missing)}/{len(keys)} covered, {len(missing)} missing")
    out = pathlib.Path(__file__).parent / "missing.json"
    if missing:
        out.write_text(json.dumps(missing, indent=1, ensure_ascii=False), encoding="utf-8")
        print("wrote", out)
    else:
        out.unlink(missing_ok=True)


if __name__ == "__main__":
    main()
