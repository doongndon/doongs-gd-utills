#!/usr/bin/env python3
"""Daily Challenge 모드의 한국어 비트맵 글꼴을 굽는다.

GD 의 글꼴에는 한글이 없다. 모드의 한국어 글(src/Text.cpp 의 문자열)에 쓰인 글자와
기본 라틴 글자만 골라 주아체(ko-patch/fonts/bmjua.ttf, SIL OFL 1.1)로 굽는다.
흰 글자에 검은 테두리(daily-ko, GD 의 bigFont 자리)와 금색 글자(daily-ko-gold,
goldFont 자리) 두 벌을, 화질 세 벌(sd/hd/uhd)로 낸다.

Text.cpp 의 한국어 글을 바꾸면 이 도구를 다시 돌려야 한다. 빠진 글자는 빈칸으로
그려지므로, 도구는 글꼴에 없는 글자가 있으면 멈춘다.

    python3 daily/tools/make_font.py
"""
import math
import pathlib

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = pathlib.Path(__file__).resolve().parent
DAILY = HERE.parent
TTF = DAILY.parent / "ko-patch" / "fonts" / "bmjua.ttf"
SOURCES = [DAILY / "src" / "Text.cpp"]
OUT = DAILY / "resources" / "fonts"

BASE_SIZE = 64
VARIANTS = (("", 4), ("-hd", 2), ("-uhd", 1))
OUTLINE = (0, 0, 0)

# (이름, 위쪽 색, 아래쪽 색)
STYLES = [
    ("daily-ko", (255, 255, 255), (255, 255, 255)),
    ("daily-ko-gold", (255, 240, 150), (255, 168, 28)),
]


def string_literals(source: str) -> list[str]:
    """C++ 소스에서 문자열 상수만 뽑는다. 주석 속의 한글은 넣지 않는다."""
    out = []
    i, n = 0, len(source)
    while i < n:
        c = source[i]
        if source.startswith("//", i):
            i = source.find("\n", i)
            if i < 0:
                break
        elif source.startswith("/*", i):
            i = source.find("*/", i + 2)
            if i < 0:
                break
            i += 2
        elif c == "'":
            i += 1
            while i < n and source[i] != "'":
                i += 2 if source[i] == "\\" else 1
            i += 1
        elif c == '"':
            i += 1
            start = i
            while i < n and source[i] != '"':
                i += 2 if source[i] == "\\" else 1
            out.append(source[start:i])
            i += 1
        else:
            i += 1
    return out


def needed_codepoints() -> list[int]:
    points = set(range(32, 127))
    for path in SOURCES:
        for literal in string_literals(path.read_text(encoding="utf-8")):
            for ch in literal:
                if ord(ch) >= 0x80:
                    points.add(ord(ch))
    return sorted(points)


def glyph_mask(font, char, size):
    pad = size
    canvas = Image.new("L", (size * 3, size * 3), 0)
    ImageDraw.Draw(canvas).text((pad, pad), char, font=font, fill=255)
    box = canvas.getbbox()
    if not box:
        return None, None
    return canvas.crop(box), (box[0] - pad, box[1] - pad)


def vertical(size, top, bottom, ascent):
    """글자 높이에 맞춘 세로 그라데이션. 줄의 맨 위에서 기준선까지 색이 바뀐다."""
    w, h = size
    img = Image.new("RGBA", (w, h))
    px = img.load()
    for y in range(h):
        t = min(1.0, max(0.0, y / max(1, ascent)))
        col = tuple(round(a + (b - a) * t) for a, b in zip(top, bottom))
        for x in range(w):
            px[x, y] = (*col, 255)
    return img


def build(name, top, bottom, size, suffix):
    font = ImageFont.truetype(str(TTF), size)
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
            missing.append(cp)
            continue
        grown = Image.new("L", (mask.width + outline * 2, mask.height + outline * 2), 0)
        grown.paste(mask, (outline, outline))
        halo = grown.filter(ImageFilter.MaxFilter(outline * 2 + 1))

        image = Image.new("RGBA", grown.size, (0, 0, 0, 0))
        image.paste(Image.new("RGBA", grown.size, (*OUTLINE, 255)), (0, 0), halo)
        # 금색은 줄 기준으로 위가 밝고 아래가 진하다. 글자 판의 위치(origin)만큼 밀어 맞춘다.
        fill = vertical((grown.width, grown.height + ascent), top, bottom, ascent)
        shift = max(0, origin[1] - outline)
        fill = fill.crop((0, shift, grown.width, shift + grown.height))
        image.paste(fill, (0, 0), grown)

        glyphs.append({
            "id": cp,
            "image": image,
            "xoffset": origin[0] - outline,
            "yoffset": origin[1] - outline,
            "xadvance": round(font.getlength(char)),
        })

    if missing:
        shown = "".join(chr(cp) for cp in missing)
        raise SystemExit(f"{name}{suffix}: 글꼴에 없는 글자 {len(missing)}개 - {shown}")

    area = sum(g["image"].width * g["image"].height for g in glyphs)
    width = 1 << max(7, math.ceil(math.log2(math.sqrt(area * 1.2))))
    tall = sorted(glyphs, key=lambda g: -g["image"].height)
    while True:
        x = y = row = 0
        for g in tall:
            w, h = g["image"].size
            if x + w + 1 > width:
                x, y, row = 0, y + row + 1, 0
            g["x"], g["y"] = x, y
            x += w + 1
            row = max(row, h)
        height = y + row
        if height <= width * 2 or width >= 4096:
            break
        width *= 2
    height = (height + 3) // 4 * 4

    sheet = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    for g in glyphs:
        sheet.paste(g["image"], (g["x"], g["y"]))
    OUT.mkdir(parents=True, exist_ok=True)
    sheet.save(OUT / f"{name}{suffix}.png", optimize=True)

    lines = [
        f'char id=32 x=0 y=0 width=0 height=0 xoffset=0 yoffset=0 '
        f'xadvance={round(font.getlength(" "))} page=0 chnl=0',
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
    return width, height, len(glyphs)


def main():
    points = needed_codepoints()
    korean = [cp for cp in points if cp >= 0x80]
    print(f"{len(points)} glyphs, {len(korean)} beyond ASCII")
    for name, top, bottom in STYLES:
        for suffix, divisor in VARIANTS:
            w, h, count = build(name, top, bottom, BASE_SIZE // divisor, suffix)
            print(f"{name}{suffix:5} {w}x{h} {count} glyphs")


if __name__ == "__main__":
    main()
