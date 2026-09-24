#!/usr/bin/env python3
"""neodgm.ttf 에서 굵은 판본을 만든다. (pip install pillow fonttools)

둥근모꼴은 16픽셀 격자 위에 그려진 글꼴이고, 트루타입으로 옮겨진 지금도 16픽셀로
찍으면 회색 없이 딱 떨어지는 흑백이 나온다. 그래서 굵게 만드는 방법이 단순하다.
글자를 16픽셀 격자로 되돌리고, 켜진 칸을 오른쪽으로 한 칸씩 번지게 한 뒤, 다시
사각형들의 모음으로 글꼴을 짜면 된다. 획이 한 픽셀씩 두꺼워진다.

이렇게 만든 트루타입을 Geode 에 넘기면 아틀라스를 굽는 일은 Geode 가 그대로 한다.
해상도별 판본 같은 까다로운 부분을 건드리지 않아도 되는 것이 이 방식의 이점이다.
"""

import argparse
import json
import pathlib

from PIL import Image, ImageDraw, ImageFont
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE = ROOT / "fonts" / "neodgm.ttf"
TARGET = ROOT / "fonts" / "doongpixel.ttf"
TRANSLATIONS = ROOT / "translations" / "ko.json"

GRID = 16                 # 글꼴이 설계된 격자 크기(픽셀)
UNITS_PER_EM = 1024
PIXEL = UNITS_PER_EM // GRID
ASCENT_PX = 12
DESCENT_PX = 4
MARGIN = GRID             # 글자가 격자 밖으로 조금 나가도 담기도록 둘러둔 여백


def needed_codepoints() -> list[int]:
    translations = json.loads(TRANSLATIONS.read_text(encoding="utf-8"))
    used = {ord(c) for text in translations.values() for c in text}
    return sorted(used | set(range(32, 127)) | {0x2022})


def pixels_of(char: str, font: ImageFont.FreeTypeFont, bold: int) -> set[tuple[int, int]]:
    canvas = Image.new("L", (GRID + MARGIN * 2, GRID + MARGIN * 2), 0)
    ImageDraw.Draw(canvas).text((MARGIN, MARGIN), char, font=font, fill=255)

    on = {
        (x - MARGIN, y - MARGIN)
        for y in range(canvas.height)
        for x in range(canvas.width)
        if canvas.getpixel((x, y)) > 127
    }
    # 오른쪽으로 번지게 해서 획을 두껍게 한다. 세로로도 번지게 하면 가로획과
    # 세로획의 두께가 따로 놀아서, 픽셀 글꼴에서는 가로로만 번지는 편이 낫다.
    for step in range(1, bold + 1):
        on |= {(x + step, y) for x, y in set(on)}
    return on


def rows_to_rects(pixels: set[tuple[int, int]]) -> list[tuple[int, int, int]]:
    # 한 줄에서 이어진 칸들을 사각형 하나로 묶는다. 칸마다 사각형을 만들면
    # 윤곽선이 수천 개가 되어 글꼴이 무거워진다.
    rects = []
    for row in sorted({y for _, y in pixels}):
        columns = sorted(x for x, y in pixels if y == row)
        start = previous = columns[0]
        for column in columns[1:]:
            if column == previous + 1:
                previous = column
                continue
            rects.append((start, previous, row))
            start = previous = column
        rects.append((start, previous, row))
    return rects


def build_glyph(pixels: set[tuple[int, int]]) -> TTGlyphPen:
    pen = TTGlyphPen(None)
    for left, right, row in rows_to_rects(pixels):
        x0 = left * PIXEL
        x1 = (right + 1) * PIXEL
        y1 = (ASCENT_PX - row) * PIXEL
        y0 = y1 - PIXEL
        # 시계 방향으로 감아 두면 사각형끼리 겹쳐도 비지 않고 채워진다.
        pen.moveTo((x0, y0))
        pen.lineTo((x0, y1))
        pen.lineTo((x1, y1))
        pen.lineTo((x1, y0))
        pen.closePath()
    return pen


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bold", type=int, default=1, help="번지게 할 픽셀 수")
    args = parser.parse_args()

    source = TTFont(SOURCE)
    source_cmap = source.getBestCmap()
    source_widths = source["hmtx"].metrics
    raster = ImageFont.truetype(str(SOURCE), GRID)

    glyphs = {".notdef": TTGlyphPen(None).glyph()}
    widths = {".notdef": GRID // 2 * PIXEL}
    cmap = {}

    for codepoint in needed_codepoints():
        name = f"uni{codepoint:04X}"
        pixels = pixels_of(chr(codepoint), raster, args.bold)
        glyphs[name] = build_glyph(pixels).glyph()

        advance = source_widths[source_cmap[codepoint]][0]
        # 번진 만큼 자리를 넓혀 주지 않으면 옆 글자를 파고든다.
        widths[name] = advance + args.bold * PIXEL
        cmap[codepoint] = name

    builder = FontBuilder(UNITS_PER_EM, isTTF=True)
    builder.setupGlyphOrder(list(glyphs))
    builder.setupCharacterMap(cmap)
    builder.setupGlyf(glyphs)
    builder.setupHorizontalMetrics({name: (widths[name], 0) for name in glyphs})
    builder.setupHorizontalHeader(ascent=ASCENT_PX * PIXEL, descent=-DESCENT_PX * PIXEL)
    # OFL 은 원본의 예약 이름(NeoDunggeunmo, Neo둥근모)을 파생 글꼴에 쓰지
    # 못하게 한다. 그래서 다른 이름을 붙인다.
    builder.setupNameTable({
        "familyName": "Doong Pixel",
        "styleName": "Regular",
        "psName": "DoongPixel",
    })
    builder.setupOS2(sTypoAscender=ASCENT_PX * PIXEL, sTypoDescender=-DESCENT_PX * PIXEL)
    builder.setupPost()
    # 만든 시각이 파일에 박히면 돌릴 때마다 내용이 달라져서, CI 가 변경으로
    # 오해한다. 시각을 고정해 같은 입력이면 같은 파일이 나오게 한다.
    builder.font["head"].created = 0
    builder.font["head"].modified = 0
    builder.save(TARGET)

    print(f"{TARGET.name} written: {len(cmap)} glyphs, bold={args.bold}px")


if __name__ == "__main__":
    main()
