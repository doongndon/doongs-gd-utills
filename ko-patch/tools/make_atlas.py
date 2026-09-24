#!/usr/bin/env python3
"""비트맵 글꼴(.fnt + .png)을 직접 굽는다. (pip install pillow)

Geode 의 글꼴 생성기는 글자에 단색 하나만 입힌다. GD 의 글꼴은 흰 획에 검은
테두리를 두른 두 가지 색이라, 그걸 맞추려면 아틀라스를 직접 만들어야 한다.

Geode 가 만들던 것과 같은 모양으로 낸다. 해상도 세 벌(sd/hd/uhd)을 각각
.fnt 와 .png 로 내보내고, .fnt 안의 page 이름에는 꼬리표를 붙이지 않는다.
어느 판본을 쓸지는 cocos 가 화질 설정을 보고 스스로 고르기 때문이다.
"""

import json
import math
import pathlib
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from glyphs import ROOT, needed_codepoints

OUT = ROOT / "resources"
BASE_SIZE = 64
VARIANTS = (("", 4), ("-hd", 2), ("-uhd", 1))   # Geode 와 같은 배수

WHITE = (255, 255, 255)
GOLD = (255, 193, 43)
OUTLINE = (0, 0, 0)

FONTS = [
    ("jua", "bmjua.ttf", WHITE),
    ("jua-gold", "bmjua.ttf", GOLD),
    ("neodgm", "neodgm.ttf", WHITE),
    ("neodgm-gold", "neodgm.ttf", GOLD),
]


def glyph_mask(font, char, size):
    """글자 하나를 흑백 판으로 찍는다. 원점은 줄의 맨 윗변이다."""
    pad = size
    canvas = Image.new("L", (size * 3, size * 3), 0)
    ImageDraw.Draw(canvas).text((pad, pad), char, font=font, fill=255)
    box = canvas.getbbox()
    if not box:
        return None, None
    # 원점(pad, pad) 기준으로 되돌린다
    return canvas.crop(box), (box[0] - pad, box[1] - pad)


def build(name, ttf, colour, size, suffix):
    font = ImageFont.truetype(str(ROOT / "fonts" / ttf), size)
    ascent, descent = font.getmetrics()
    outline = max(1, round(size / 16))

    glyphs = []
    missing = []
    for cp in needed_codepoints():
        char = chr(cp)
        if cp == 32:
            continue
        mask, origin = glyph_mask(font, char, size)
        if mask is None:
            # 글꼴이 그리지 못한 글자다. 그냥 넘기면 화면에 빈 칸으로 나와서
            # 알아채기 어렵다. 목록을 모아 두었다가 끝에서 멈춘다.
            missing.append(cp)
            continue
        # 테두리는 획을 사방으로 부풀린 것이다. 부푼 만큼 자리도 넓어진다.
        grown = Image.new("L", (mask.width + outline * 2, mask.height + outline * 2), 0)
        grown.paste(mask, (outline, outline))
        halo = grown.filter(ImageFilter.MaxFilter(outline * 2 + 1))

        image = Image.new("RGBA", grown.size, (0, 0, 0, 0))
        image.paste(Image.new("RGBA", grown.size, (*OUTLINE, 255)), (0, 0), halo)
        image.paste(Image.new("RGBA", grown.size, (*colour, 255)), (0, 0), grown)

        glyphs.append({
            "id": cp,
            "image": image,
            "xoffset": origin[0] - outline,
            "yoffset": origin[1] - outline,
            "xadvance": round(font.getlength(char)),
        })

    # 선반 쌓기. 높은 글자부터 줄을 세워 가로로 채운다. 넓이는 대략 정사각이
    # 되도록 고르고, 그래도 4096 을 넘으면 넓혀서 다시 쌓는다.
    area = sum(g["image"].width * g["image"].height for g in glyphs)
    width = 1 << max(8, math.ceil(math.log2(math.sqrt(area * 1.15))))
    tall = sorted(glyphs, key=lambda g: -g["image"].height)
    while True:
        x = y = row = 0
        for g in tall:
            w, h = g["image"].size
            if x + w > width:
                x, y, row = 0, y + row, 0
            g["x"], g["y"] = x, y
            x += w
            row = max(row, h)
        height = y + row
        if height <= 4096 or width >= 4096:
            break
        width *= 2
    if height > 4096 or width > 4096:
        raise SystemExit(f"{name}{suffix}: {width}x{height} 는 4096 한계를 넘습니다")
    height = (height + 3) // 4 * 4

    sheet = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    for g in glyphs:
        sheet.paste(g["image"], (g["x"], g["y"]))

    OUT.mkdir(parents=True, exist_ok=True)
    # 글자는 색이 둘(획과 테두리)뿐이고 나머지는 경계의 부드러운 층이다.
    # 32색 팔레트면 그 층이 눈에 띄게 상하지 않으면서 용량은 1/4 이 된다.
    sheet.quantize(colors=32, method=Image.Quantize.FASTOCTREE).save(
        OUT / f"{name}{suffix}.png", optimize=True)

    if missing:
        shown = "".join(chr(cp) for cp in missing[:40])
        raise SystemExit(
            f"{name}{suffix}: {ttf} 가 그리지 못하는 글자 {len(missing)}개 — {shown}")

    lines = [
        f'char id=32 x=0 y=0 width=0 height=0 xoffset=0 yoffset=0 '
        f'xadvance={round(font.getlength(" "))} page=0 chnl=0'
    ]
    for g in sorted(glyphs, key=lambda g: g["id"]):
        lines.append(
            f'char id={g["id"]} x={g["x"]} y={g["y"]} '
            f'width={g["image"].width} height={g["image"].height} '
            f'xoffset={g["xoffset"]} yoffset={g["yoffset"]} '
            f'xadvance={g["xadvance"]} page=0 chnl=0'
        )

    (OUT / f"{name}{suffix}.fnt").write_text(
        f'info face="{name}" size={size} bold=0 italic=0 charset="" unicode=1 '
        f'stretchH=100 smooth=1 aa=1 padding=0,0,0,0 spacing=1,1\n'
        f'common lineHeight={ascent + descent} base={ascent} '
        f'scaleW={width} scaleH={height} pages=1 packed=0\n'
        f'page id=0 file="{name}.png"\n'
        f'chars count={len(lines)}\n' + "\n".join(lines) + "\n"
        f'kernings count=0\n',
        encoding="utf-8")
    return width, height, (OUT / f"{name}{suffix}.png").stat().st_size


def main() -> None:
    total = 0
    for name, ttf, colour in FONTS:
        for suffix, divisor in VARIANTS:
            w, h, size = build(name, ttf, colour, BASE_SIZE // divisor, suffix)
            total += size
            print(f"{name}{suffix:5} {w}x{h}  {size // 1024}KB")
    print(f"total png {total // 1024}KB")


if __name__ == "__main__":
    main()
