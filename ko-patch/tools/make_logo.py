#!/usr/bin/env python3
"""336x336 logo.png 를 만든다. (pillow 필요: pip install pillow)

Neo둥근모는 16픽셀 비트맵 글꼴을 트루타입으로 옮긴 것이라, 16의 배수 크기로
그려야 픽셀이 뭉개지지 않는다. 그래서 글자 크기를 208(=16x13)로 잡았다.
"""

import pathlib

from PIL import Image, ImageDraw, ImageFont

ROOT = pathlib.Path(__file__).resolve().parent.parent
SIZE = 336
RADIUS = 74
GLYPH_SIZE = 16 * 13

BG_TOP = (0x12, 0x20, 0x1A)
BG_BOTTOM = (0x06, 0x0C, 0x09)
FG = (0x7C, 0xF5, 0xB4)


def main() -> None:
    image = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)

    for y in range(SIZE):
        t = y / (SIZE - 1)
        colour = tuple(
            int(round(a + (b - a) * t)) for a, b in zip(BG_TOP, BG_BOTTOM)
        )
        draw.line([(0, y), (SIZE, y)], fill=(*colour, 255))

    corners = Image.new("L", (SIZE, SIZE), 0)
    ImageDraw.Draw(corners).rounded_rectangle([0, 0, SIZE - 1, SIZE - 1], RADIUS, fill=255)
    image.putalpha(corners)

    font = ImageFont.truetype(str(ROOT / "fonts" / "neodgm.ttf"), GLYPH_SIZE)
    glyph = ImageDraw.Draw(image)
    box = glyph.textbbox((0, 0), "한", font=font)
    glyph.text(
        ((SIZE - (box[2] - box[0])) / 2 - box[0], (SIZE - (box[3] - box[1])) / 2 - box[1]),
        "한",
        font=font,
        fill=(*FG, 255),
    )

    image.save(ROOT / "logo.png")
    print("logo.png written")


if __name__ == "__main__":
    main()
