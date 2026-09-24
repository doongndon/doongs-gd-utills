#!/usr/bin/env python3
"""ko.json 에 실제로 쓰인 글자만 글꼴 아틀라스에 굽도록 charset 을 맞춘다.

한글 음절은 유니코드에 11,172자가 있다. 그걸 전부 구우면 텍스처가 수천 픽셀
크기가 되어 폰에서 부담스럽다. 번역문에 등장하는 글자는 그 중 극히 일부이므로,
쓰는 글자만 골라 담는다. 대신 번역을 추가하고 이 스크립트를 잊으면 그 글자가
화면에서 사라지므로, CI 가 --check 로 어긋남을 잡는다.
"""

import argparse
import json
import pathlib
import sys

from glyphs import ROOT, needed_codepoints

MOD_JSON = ROOT / "mod.json"


def to_charset(codepoints: list[int]) -> str:
    # 이어지는 번호는 "시작-끝" 으로 접어서 짧게 만든다.
    parts = []
    ordered = list(codepoints)
    start = previous = ordered[0]
    for point in ordered[1:]:
        if point == previous + 1:
            previous = point
            continue
        parts.append(str(start) if start == previous else f"{start}-{previous}")
        start = previous = point
    parts.append(str(start) if start == previous else f"{start}-{previous}")
    return ",".join(parts)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="고치지 않고 어긋남만 알린다")
    args = parser.parse_args()

    mod = json.loads(MOD_JSON.read_text(encoding="utf-8"))
    font = mod["resources"]["fonts"]["neodgm"]
    expected = to_charset(needed_codepoints())

    if font.get("charset") == expected:
        print("charset is in sync")
        return 0

    if args.check:
        print(
            "charset is out of date - run ko-patch/tools/sync_charset.py",
            file=sys.stderr,
        )
        return 1

    font["charset"] = expected
    MOD_JSON.write_text(json.dumps(mod, indent=4, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"charset updated ({len(needed_codepoints())} codepoints)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
