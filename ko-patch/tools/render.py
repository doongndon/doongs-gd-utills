#!/usr/bin/env python3
"""Translator.cpp 가 실제로 내놓는 한국어를 그대로 만들어 본다. 빈칸까지 채운다."""
import importlib.util, pathlib, re, sys

HERE = pathlib.Path(__file__).parent
spec = importlib.util.spec_from_file_location("cov", HERE / "coverage.py")
cov = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cov)
SLOT = re.compile(r"\{([0-9]*)\}")

PARTICLES = [("을(를)", "을", "를"), ("를(을)", "을", "를"),
             ("이(가)", "이", "가"), ("가(이)", "이", "가"),
             ("은(는)", "은", "는"), ("는(은)", "은", "는"),
             ("와(과)", "과", "와"), ("과(와)", "과", "와"),
             ("으로(로)", "으로", "로"), ("로(으로)", "으로", "로")]
DIGIT_END = (1, 1, 0, 1, 0, 0, 1, 1, 1, 0)


def _ends(text, before):
    at = before
    while True:
        while at > 0 and text[at - 1] == " ":
            at -= 1
        if at > 0 and text[at - 1] == ">":
            open_ = text.rfind("<", 0, at - 1)
            if open_ == -1:
                break
            at = open_
            continue
        break
    if at == 0:
        return 2
    c = text[at - 1]
    if "\uac00" <= c <= "\ud7a3":
        return 1 if (ord(c) - 0xAC00) % 28 else 0
    if c.isdigit():
        return DIGIT_END[int(c)]
    return 2


def _rieul(text, before):
    at = before
    while at > 0 and text[at - 1] == " ":
        at -= 1
    if at == 0:
        return False
    c = text[at - 1]
    return "\uac00" <= c <= "\ud7a3" and (ord(c) - 0xAC00) % 28 == 8


def choose_particles(text):
    for both, with_end, no_end in PARTICLES:
        at = text.find(both)
        while at != -1:
            kind = _ends(text, at)
            if kind == 2:
                at = text.find(both, at + len(both))
                continue
            pick = with_end if kind == 1 else no_end
            if with_end == "으로" and kind == 1 and _rieul(text, at):
                pick = no_end
            begin = at
            while begin > 0 and text[begin - 1] == " ":
                begin -= 1
            text = text[:begin] + pick + text[at + len(both):]
            at = text.find(both, begin + len(pick))
    return text



def captures_for(text, segments):
    if not text.startswith(segments[0]) or not text.endswith(segments[-1]):
        return None
    cursor = len(segments[0])
    out = []
    for i in range(1, len(segments)):
        seg = segments[i]
        if i + 1 == len(segments):
            at = len(text) - len(seg)
            if at < cursor:
                return None
        else:
            # 빈 조각은 그 자리에서 곧바로 걸린다. coverage.py 와 같은 규칙.
            at = cursor if seg == "" else text.find(seg, cursor)
            if at == -1:
                return None
        out.append(text[cursor:at])
        cursor = at + len(seg)
    return out or None


def render(text, exact, patterns, depth=0, origin=None):
    if text in exact:
        return choose_particles(exact[text])
    if depth > 1:
        return None
    for segments, numeric, korean in patterns:
        caps = captures_for(text, segments)
        if caps is None:
            continue
        if not all(cov.is_plain_ascii(c) for c in caps):
            continue
        # 값 하나가 제 색을 입고 오는 것은 막지 않는다. 색표가 여러 벌이면
        # 문장을 삼킨 것이다. Translator.cpp 와 같은 규칙.
        if any(cov.too_many_tags(c) for c in caps):
            continue
        # 편 글로 맞춘 것이라면, 조각 안에서 줄이 바뀌었는지 본다. 맨 앞이나
        # 맨 뒤의 줄바꿈은 틀의 이음매에서 끊긴 것이니 괜찮다.
        if origin is not None and len(origin) == len(text):
            spans, cur = [], len(segments[0])
            for i in range(1, len(segments)):
                seg = segments[i]
                at = len(text) - len(seg) if i + 1 == len(segments) else text.find(seg, cur)
                spans.append((cur, at))
                cur = at + len(seg)
            if any(any(j not in (0, b - a - 1)
                       for j, ch in enumerate(origin[a:b]) if ch == "\n")
                   for a, b in spans):
                continue
        if any(n and i < len(caps) and not cov.is_numeric(caps[i])
               for i, n in enumerate(numeric)):
            continue
        if any(w and i < len(caps) and not cov.is_numeric(caps[i])
               for i, w in cov.numeric_slots(korean)):
            continue
        out, last, nxt = [], 0, 0
        for m in SLOT.finditer(korean):
            out.append(korean[last:m.start()])
            index = int(m.group(1)) if m.group(1) else nxt
            if not m.group(1):
                nxt += 1
            if index < len(caps):
                # 먼저 있는 그대로 찾아본다. 못 찾았을 때만 앞뒤 빈칸을
                # 떼고 다시 찾는다. Translator.cpp 와 같은 차례다.
                whole = caps[index]
                piece = render(whole, exact, patterns, depth + 1)
                raw = whole
                if piece is None:
                    raw = whole.strip(" \t\n")
                    if raw != whole:
                        piece = render(raw, exact, patterns, depth + 1)
                out.append(piece if piece is not None else raw)
            last = m.end()
        out.append(korean[last:])
        return choose_particles("".join(out))

    # 자리가 좁으면 GD 가 줄바꿈을 끼워 넣는다. 펴서 한 번 더 찾아본다.
    if origin is None and "\n" in text:
        return render(text.replace("\n", " "), exact, patterns, depth, text)
    return None


if __name__ == "__main__":
    exact, patterns = cov.load()
    for line in sys.argv[1:]:
        print(repr(line[:60]), "->", repr(render(line, exact, patterns)))
