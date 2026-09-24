"""아틀라스에 구워야 할 글자 목록. 두 도구가 같은 답을 봐야 하므로 한 곳에 둔다."""

import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
TRANSLATIONS = ROOT / "translations" / "ko.json"

# 번역문에 없더라도 GD 가 숫자와 기호를 섞어 쓰므로 기본 라틴 영역은 항상 넣는다.
ALWAYS = set(range(32, 127)) | {0x2022}


def korean_texts() -> list[str]:
    data = json.loads(TRANSLATIONS.read_text(encoding="utf-8"))
    texts = list(data["exact"].values())
    # 틀 문장의 {} 는 원문이 그대로 들어가는 자리라 글자가 아니다.
    texts += [to.replace("{}", "") for to in data["patterns"].values()]
    return texts


def needed_codepoints() -> list[int]:
    used = {ord(c) for text in korean_texts() for c in text}
    return sorted(ALWAYS | used)
