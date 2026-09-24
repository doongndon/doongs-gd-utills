"""아틀라스에 구워야 할 글자 목록. 두 도구가 같은 답을 봐야 하므로 한 곳에 둔다."""

import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
TRANSLATIONS = ROOT / "translations" / "ko.json"
HANGUL_SET = pathlib.Path(__file__).resolve().parent / "hangul-set.txt"

# 번역문에 없더라도 GD 가 숫자와 기호를 섞어 쓰므로 기본 라틴 영역은 항상 넣는다.
# 가운뎃점(U+2022)은 두 글꼴 모두 cmap 에만 있고 획이 비어 있어 뺀다.
ALWAYS = set(range(32, 127))


def korean_texts() -> list[str]:
    data = json.loads(TRANSLATIONS.read_text(encoding="utf-8"))
    texts = list(data["exact"].values())
    # 틀 문장의 {} 는 원문이 그대로 들어가는 자리라 글자가 아니다.
    texts += [to.replace("{}", "") for to in data["patterns"].values()]
    return texts


def hangul_set() -> set[int]:
    # 우리가 쓴 글자만 굽던 시절에는 번역을 늘릴 때마다 목록이 흔들렸다. 이제는
    # 상용 한글을 통째로 담는다. 우리가 쓰지 않은 한국어 - 레벨 이름이나 자동
    # 번역이 돌려준 문장 - 도 그려야 하기 때문이다.
    points: set[int] = set()
    for line in HANGUL_SET.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        for part in line.split(","):
            if "-" in part:
                first, last = part.split("-")
                points.update(range(int(first), int(last) + 1))
            else:
                points.add(int(part))
    return points


def needed_codepoints() -> list[int]:
    used = {ord(c) for text in korean_texts() for c in text}
    return sorted(ALWAYS | hangul_set() | used)
