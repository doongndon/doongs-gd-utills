#!/usr/bin/env python3
"""Daily Challenge 모드의 그림을 만든다.

카드 앞면과 뒷면, 카드 그림 10장, HUD 부품, 메뉴 단추 그림을 그려 daily/resources/ 에
넣는다. 모두 UHD 크기(게임 좌표 1 = 4 픽셀)로 그리고, Geode CLI 가 묶을 때 HD 와
보통 크기를 스스로 만든다(mod.json 의 resources.sprites).

글자는 그리지 않는다. 카드 이름과 설명, 점수는 게임 안에서 GD 글꼴로 얹는다.
그 자리는 아래 CARD_* 값에 적어 두었고 src/ui/CardFace.cpp 가 같은 값을 쓴다.

    python3 daily/tools/make_art.py
"""
import math
import os

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageFilter

OUT = os.path.join(os.path.dirname(__file__), "..", "resources")

# 게임 좌표 한 칸이 UHD 에서 몇 픽셀인지.
UHD = 4
# 매끄럽게 그리려고 크게 그린 뒤 줄인다.
SS = 4

# 카드 한 장의 크기(게임 좌표). CardFace.cpp 의 CARD_W, CARD_H 와 같아야 한다.
CARD_W, CARD_H = 120, 168
# 카드 위 자리들(게임 좌표, 아래가 0). CardFace.cpp 와 같아야 한다.
CARD_RIBBON_Y = 153
CARD_MEDAL = (60, 111, 29)  # 가운데 x, y, 반지름
CARD_NAME_Y = 70
CARD_NAME_H = 19
CARD_TEXT_TOP, CARD_TEXT_BOTTOM = 59, 25
CARD_POINTS_Y = 13

INK = (14, 12, 22, 255)
GOLD_TOP = (255, 226, 120)
GOLD_BOTTOM = (196, 128, 24)

KINDS = {
    "help": {
        "light": (120, 255, 205),
        "mid": (40, 205, 150),
        "dark": (10, 96, 80),
        "deep_top": (16, 58, 54),
        "deep_bottom": (6, 26, 26),
    },
    "curse": {
        "light": (255, 150, 210),
        "mid": (235, 60, 150),
        "dark": (110, 18, 74),
        "deep_top": (60, 16, 48),
        "deep_bottom": (24, 6, 20),
    },
}


# ---------------------------------------------------------------- 그리기 도구

def canvas(w, h):
    return Image.new("RGBA", (w, h), (0, 0, 0, 0))


def mask(w, h):
    return Image.new("L", (w, h), 0)


def bezier(points, steps=24):
    """제어점으로 이어진 곡선 위의 점들."""
    out = []
    n = len(points) - 1
    for i in range(steps + 1):
        t = i / steps
        x = y = 0.0
        for k, (px, py) in enumerate(points):
            c = math.comb(n, k) * (1 - t) ** (n - k) * t ** k
            x += c * px
            y += c * py
        out.append((x, y))
    return out


def gradient(w, h, top, bottom, angle=90):
    """세로(90) 또는 비스듬한 선형 그라데이션."""
    ys, xs = np.mgrid[0:h, 0:w].astype(np.float32)
    rad = math.radians(angle)
    proj = xs * math.cos(rad) + ys * math.sin(rad)
    proj = (proj - proj.min()) / max(1e-6, proj.max() - proj.min())
    top = np.array(top[:3], np.float32)
    bottom = np.array(bottom[:3], np.float32)
    rgb = top[None, None, :] * (1 - proj[..., None]) + bottom[None, None, :] * proj[..., None]
    alpha = np.full((h, w, 1), 255, np.float32)
    return Image.fromarray(np.concatenate([rgb, alpha], axis=2).astype(np.uint8), "RGBA")


def radial(w, h, center, radius, inner, outer, power=1.0):
    ys, xs = np.mgrid[0:h, 0:w].astype(np.float32)
    d = np.sqrt((xs - center[0]) ** 2 + (ys - center[1]) ** 2) / radius
    d = np.clip(d, 0, 1) ** power
    inner = np.array(inner, np.float32)
    outer = np.array(outer, np.float32)
    if inner.shape[0] == 3:
        inner = np.append(inner, 255)
    if outer.shape[0] == 3:
        outer = np.append(outer, 255)
    px = inner[None, None, :] * (1 - d[..., None]) + outer[None, None, :] * d[..., None]
    return Image.fromarray(px.astype(np.uint8), "RGBA")


def solid(w, h, color):
    c = color if len(color) == 4 else (*color, 255)
    return Image.new("RGBA", (w, h), c)


def paint(base, layer, m):
    """layer 를 m 모양으로 잘라 base 위에 얹는다."""
    cut = layer.copy()
    a = ImageChops.multiply(cut.getchannel("A"), m)
    cut.putalpha(a)
    return Image.alpha_composite(base, cut)


def grow(m, px):
    """모양을 px 만큼 부풀린다(테두리용).

    3x3 네모 팽창과 십자 팽창을 번갈아 하면 팔각형에 가까운, 거의 둥근 팽창이 된다.
    네모 팽창만 하면 둥근 모양의 테두리가 모서리에서 각져 보인다."""
    steps = int(round(px))
    if steps <= 0:
        return m
    arr = np.array(m, np.uint8)
    for i in range(steps):
        if i % 2 == 0:
            arr = np.array(Image.fromarray(arr, "L").filter(ImageFilter.MaxFilter(3)), np.uint8)
        else:
            out = arr.copy()
            out[1:, :] = np.maximum(out[1:, :], arr[:-1, :])
            out[:-1, :] = np.maximum(out[:-1, :], arr[1:, :])
            out[:, 1:] = np.maximum(out[:, 1:], arr[:, :-1])
            out[:, :-1] = np.maximum(out[:, :-1], arr[:, 1:])
            arr = out
    return Image.fromarray(arr, "L")


def shrink(m, px):
    inv = ImageChops.invert(m)
    return ImageChops.invert(grow(inv, px))


def blur(m, r):
    return m.filter(ImageFilter.GaussianBlur(r))


def poly_mask(w, h, points):
    m = mask(w, h)
    ImageDraw.Draw(m).polygon([(float(x), float(y)) for x, y in points], fill=255)
    return m


def ellipse_mask(w, h, box):
    m = mask(w, h)
    ImageDraw.Draw(m).ellipse(box, fill=255)
    return m


def rrect_mask(w, h, box, r):
    m = mask(w, h)
    ImageDraw.Draw(m).rounded_rectangle(box, radius=r, fill=255)
    return m


def line_mask(w, h, points, width):
    m = mask(w, h)
    d = ImageDraw.Draw(m)
    d.line(points, fill=255, width=int(width), joint="curve")
    r = width / 2
    for x, y in (points[0], points[-1]):
        d.ellipse((x - r, y - r, x + r, y + r), fill=255)
    return m


def star_points(cx, cy, outer, inner, n=5, rot=-90):
    pts = []
    for i in range(n * 2):
        r = outer if i % 2 == 0 else inner
        a = math.radians(rot + i * 180 / n)
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def inked(base, shape, fill_layer, ink_px, highlight=None):
    """GD 그림처럼 검은 테두리를 두르고 안을 칠한다."""
    w, h = base.size
    base = paint(base, solid(w, h, INK), grow(shape, ink_px))
    base = paint(base, fill_layer, shape)
    if highlight is not None:
        base = paint(base, highlight[0], highlight[1])
    return base


def sheen(w, h, shape, strength=90, angle=60):
    """윗왼쪽이 밝아지는 광택. 모양 안쪽에만 얹는다."""
    g = gradient(w, h, (255, 255, 255), (255, 255, 255), angle)
    ys, xs = np.mgrid[0:h, 0:w].astype(np.float32)
    rad = math.radians(angle)
    proj = xs * math.cos(rad) + ys * math.sin(rad)
    proj = (proj - proj.min()) / max(1e-6, proj.max() - proj.min())
    a = np.clip(1.0 - proj * 1.8, 0, 1) * strength
    alpha = Image.fromarray(a.astype(np.uint8), "L")
    g.putalpha(ImageChops.multiply(alpha, shrink(shape, 6 * SS)))
    return g


def finish(img, size):
    return img.resize((size, size) if isinstance(size, int) else size, Image.LANCZOS)


def save(img, name):
    path = os.path.join(OUT, name)
    img.save(path, optimize=True)
    print("wrote", os.path.relpath(path), img.size)


# ---------------------------------------------------------------- 카드 그림(아이콘)

ICON = 192  # UHD 픽셀 = 게임 48
W = ICON * SS
C = W / 2
INKW = 9 * SS


def icon_shield():
    img = canvas(W, W)
    left = bezier([(150, 130), (160, 420), (250, 560), (384, 700)])
    right = [(W - x, y) for x, y in reversed(left)]
    top = bezier([(618, 130), (500, 160), (430, 120), (384, 90), (338, 120), (268, 160), (150, 130)])
    shape = poly_mask(W, W, left + right + top)
    img = inked(img, shape, gradient(W, W, (150, 235, 255), (20, 110, 220)), INKW)
    rim = ImageChops.subtract(shape, shrink(shape, 28))
    img = paint(img, gradient(W, W, GOLD_TOP, GOLD_BOTTOM), rim)
    inner_ink = ImageChops.subtract(shrink(shape, 28), shrink(shape, 40))
    img = paint(img, solid(W, W, INK), inner_ink)
    emblem = poly_mask(W, W, star_points(C, 360, 150, 64))
    img = inked(img, emblem, gradient(W, W, (255, 255, 255), (200, 235, 255)), 7 * SS)
    img = paint(img, sheen(W, W, shape, 70), shape)
    return finish(img, ICON)


def icon_slow():
    img = canvas(W, W)
    glass = poly_mask(W, W, [(230, 160), (538, 160), (418, 384), (538, 610), (230, 610), (350, 384)])
    img = paint(img, solid(W, W, INK), grow(glass, INKW))
    img = paint(img, gradient(W, W, (210, 245, 255, 255), (130, 190, 230, 255)), glass)
    sand_top = poly_mask(W, W, [(286, 250), (482, 250), (384, 372)])
    sand_bottom = poly_mask(W, W, [(270, 600), (498, 600), (420, 520), (384, 470), (348, 520)])
    stream = rrect_mask(W, W, (374, 372, 394, 560), 10)
    sand = ImageChops.lighter(ImageChops.lighter(sand_top, sand_bottom), stream)
    img = paint(img, gradient(W, W, (255, 214, 110), (230, 140, 40)), ImageChops.multiply(sand, glass))
    img = paint(img, sheen(W, W, glass, 110), glass)
    for top in (100, 600):
        cap = rrect_mask(W, W, (190, top, 578, top + 70), 22)
        img = inked(img, cap, gradient(W, W, GOLD_TOP, GOLD_BOTTOM), INKW)
    return finish(img, ICON)


def icon_mirror():
    img = canvas(W, W)
    left = poly_mask(W, W, [(96, 384), (330, 196), (330, 572)])
    right = poly_mask(W, W, [(672, 384), (438, 196), (438, 572)])
    img = inked(img, left, gradient(W, W, (255, 200, 90), (240, 110, 20)), INKW)
    img = inked(img, right, gradient(W, W, (140, 230, 255), (40, 120, 240)), INKW)
    bar = rrect_mask(W, W, (366, 96, 402, 672), 18)
    img = inked(img, bar, solid(W, W, (255, 255, 255)), INKW)
    for y in range(150, 640, 90):
        gap = rrect_mask(W, W, (370, y, 398, y + 34), 8)
        img = paint(img, solid(W, W, (170, 190, 220)), gap)
    img = paint(img, sheen(W, W, left, 80), left)
    img = paint(img, sheen(W, W, right, 80), right)
    return finish(img, ICON)


def icon_mini():
    img = canvas(W, W)
    # 원래 크기의 점선 상자
    outer = ImageChops.subtract(rrect_mask(W, W, (110, 110, 658, 658), 40), rrect_mask(W, W, (138, 138, 630, 630), 26))
    dash = mask(W, W)
    d = ImageDraw.Draw(dash)
    for i in range(0, W, 96):
        d.rectangle((i, 0, i + 56, W), fill=255)
    dashes = ImageChops.multiply(outer, dash)
    dash_v = mask(W, W)
    d = ImageDraw.Draw(dash_v)
    for i in range(0, W, 96):
        d.rectangle((0, i, W, i + 56), fill=255)
    dashes = ImageChops.lighter(ImageChops.multiply(outer, dash), ImageChops.multiply(outer, dash_v))
    img = paint(img, solid(W, W, (255, 255, 255, 170)), dashes)
    # 작아진 큐브
    cube = rrect_mask(W, W, (264, 264, 504, 504), 24)
    img = inked(img, cube, gradient(W, W, (255, 150, 230), (210, 40, 170)), INKW)
    face = rrect_mask(W, W, (318, 318, 450, 450), 12)
    img = inked(img, face, gradient(W, W, (255, 230, 120), (255, 170, 40)), 7 * SS)
    img = paint(img, sheen(W, W, cube, 80), cube)
    # 안쪽을 가리키는 화살표 네 개
    for sx, sy in ((-1, -1), (1, -1), (-1, 1), (1, 1)):
        tip = (C + sx * 150, C + sy * 150)
        base = (C + sx * 240, C + sy * 240)
        arrow = line_mask(W, W, [base, tip], 34)
        head = poly_mask(W, W, [
            (tip[0] - sx * 4, tip[1] - sy * 4),
            (tip[0] + sx * 70, tip[1] - sy * 6),
            (tip[0] - sx * 6, tip[1] + sy * 70),
        ])
        arrow = ImageChops.lighter(arrow, head)
        img = inked(img, arrow, solid(W, W, (255, 255, 255)), 6 * SS)
    return finish(img, ICON)


def icon_fast():
    img = canvas(W, W)
    for i, x in enumerate((210, 360, 510)):
        chevron = poly_mask(W, W, [
            (x - 120, 170), (x - 20, 170), (x + 150, 384), (x - 20, 598), (x - 120, 598), (x + 50, 384),
        ])
        shade = 1 - i * 0.12
        top = tuple(int(c * shade) for c in (255, 236, 110))
        bottom = tuple(int(c * shade) for c in (240, 80, 30))
        img = inked(img, chevron, gradient(W, W, top, bottom), INKW)
        img = paint(img, sheen(W, W, chevron, 70), chevron)
    return finish(img, ICON)


def icon_dark():
    img = canvas(W, W)
    moon = ImageChops.subtract(
        ellipse_mask(W, W, (120, 110, 620, 610)),
        ellipse_mask(W, W, (270, 40, 740, 510)),
    )
    img = inked(img, moon, gradient(W, W, (255, 246, 190), (230, 170, 70), 45), INKW)
    img = paint(img, sheen(W, W, moon, 90, 30), moon)
    for (x, y, r) in ((560, 520, 70), (470, 650, 40), (640, 360, 34)):
        star = poly_mask(W, W, star_points(x, y, r, r * 0.42))
        img = inked(img, star, solid(W, W, (220, 225, 255)), 6 * SS)
    return finish(img, ICON)


def icon_shake():
    img = canvas(W, W)
    zig = [(90, 420), (210, 420), (270, 250), (360, 560), (440, 180), (520, 500), (580, 380), (680, 380)]
    line = line_mask(W, W, zig, 64)
    img = inked(img, line, gradient(W, W, (255, 210, 90), (230, 90, 30), 0), INKW)
    for side in (-1, 1):
        for k, r in enumerate((300, 360)):
            arc = mask(W, W)
            box = (C - r, C - r, C + r, C + r)
            start, end = (150, 210) if side < 0 else (-30, 30)
            ImageDraw.Draw(arc).arc(box, start, end, fill=255, width=34)
            img = inked(img, arc, solid(W, W, (255, 255, 255, 230 - k * 60)), 6 * SS)
    return finish(img, ICON)


def icon_invisible():
    img = canvas(W, W)
    head = ellipse_mask(W, W, (170, 90, 598, 518))
    body = poly_mask(W, W, [(170, 300), (598, 300), (598, 640), (170, 640)])
    waves = mask(W, W)
    d = ImageDraw.Draw(waves)
    for i in range(4):
        x0 = 170 + i * 107
        d.ellipse((x0, 590, x0 + 107, 700), fill=255)
    cut = mask(W, W)
    d = ImageDraw.Draw(cut)
    for i in range(3):
        x0 = 170 + 107 / 2 + i * 107
        d.ellipse((x0 + 8, 640, x0 + 99, 760), fill=255)
    ghost = ImageChops.subtract(ImageChops.lighter(ImageChops.lighter(head, body), waves), cut)
    ghost = ImageChops.multiply(ghost, poly_mask(W, W, [(0, 0), (W, 0), (W, 700), (0, 700)]))
    img = paint(img, solid(W, W, (14, 12, 22, 200)), grow(ghost, INKW))
    img = paint(img, gradient(W, W, (255, 255, 255, 255), (190, 180, 255, 255)), ghost)
    faded = ghost.point(lambda v: int(v * 0.35))
    img = paint(img, solid(W, W, (120, 110, 200, 255)), ImageChops.multiply(faded, poly_mask(W, W, [(384, 0), (W, 0), (W, W), (384, W)])))
    for x in (300, 468):
        eye = ellipse_mask(W, W, (x - 38, 260, x + 38, 370))
        img = paint(img, solid(W, W, INK), eye)
        glint = ellipse_mask(W, W, (x - 16, 276, x + 6, 300))
        img = paint(img, solid(W, W, (255, 255, 255)), glint)
    mouth = ellipse_mask(W, W, (350, 420, 418, 490))
    img = paint(img, solid(W, W, INK), mouth)
    return finish(img, ICON)


def icon_mute():
    img = canvas(W, W)
    speaker = poly_mask(W, W, [(110, 300), (240, 300), (420, 150), (420, 618), (240, 468), (110, 468)])
    img = inked(img, speaker, gradient(W, W, (240, 240, 250), (150, 150, 175)), INKW)
    img = paint(img, sheen(W, W, speaker, 90), speaker)
    x1 = line_mask(W, W, [(500, 270), (680, 498)], 70)
    x2 = line_mask(W, W, [(680, 270), (500, 498)], 70)
    cross = ImageChops.lighter(x1, x2)
    img = inked(img, cross, gradient(W, W, (255, 120, 110), (200, 20, 40)), INKW)
    return finish(img, ICON)


def icon_flash():
    img = canvas(W, W)
    bulb = ImageChops.lighter(
        ellipse_mask(W, W, (190, 90, 578, 478)),
        poly_mask(W, W, [(262, 400), (506, 400), (470, 540), (298, 540)]),
    )
    img = paint(img, solid(W, W, INK), grow(bulb, INKW))
    img = paint(img, gradient(W, W, (70, 70, 96), (24, 22, 40)), bulb)
    lit = ImageChops.multiply(bulb, poly_mask(W, W, [(0, 0), (W, 0), (0, W)]))
    lit = ImageChops.multiply(lit, poly_mask(W, W, [(0, 0), (520, 0), (180, 560), (0, 560)]))
    img = paint(img, gradient(W, W, (255, 248, 170), (255, 196, 40)), lit)
    img = paint(img, sheen(W, W, bulb, 60), bulb)
    filament = line_mask(W, W, [(330, 330), (360, 270), (384, 330), (408, 270), (438, 330)], 18)
    img = paint(img, solid(W, W, (255, 150, 40)), filament)
    for top in (548, 608):
        ring = rrect_mask(W, W, (290, top, 478, top + 50), 18)
        img = inked(img, ring, gradient(W, W, (230, 230, 240), (130, 130, 150)), 7 * SS)
    tip = rrect_mask(W, W, (334, 664, 434, 704), 16)
    img = inked(img, tip, solid(W, W, (90, 90, 110)), 7 * SS)
    for ang in (-160, -125, -90):
        a = math.radians(ang)
        p0 = (C + math.cos(a) * 230, 300 + math.sin(a) * 230)
        p1 = (C + math.cos(a) * 290, 300 + math.sin(a) * 290)
        ray = line_mask(W, W, [p0, p1], 34)
        img = inked(img, ray, solid(W, W, (255, 230, 90)), 6 * SS)
    return finish(img, ICON)


ICONS = {
    "shield": icon_shield,
    "slow": icon_slow,
    "mirror": icon_mirror,
    "mini": icon_mini,
    "fast": icon_fast,
    "dark": icon_dark,
    "shake": icon_shake,
    "invisible": icon_invisible,
    "mute": icon_mute,
    "flash": icon_flash,
}


# ---------------------------------------------------------------- 카드 틀

def py(pt_y, h_pt=CARD_H, scale=UHD * 2):
    """게임 좌표(아래가 0)의 y 를 그림 좌표(위가 0)로."""
    return (h_pt - pt_y) * scale


def card_face(kind):
    s = UHD * 2  # 카드는 2배로 그려 줄인다
    w, h = CARD_W * s, CARD_H * s
    pal = KINDS[kind]
    img = canvas(w, h)

    outer = rrect_mask(w, h, (0, 0, w - 1, h - 1), 22 * s // 2)
    img = paint(img, solid(w, h, INK), outer)
    border = rrect_mask(w, h, (4 * s, 4 * s, w - 4 * s, h - 4 * s), 18 * s // 2)
    img = paint(img, gradient(w, h, pal["light"], pal["dark"], 80), border)
    img = paint(img, sheen(w, h, border, 60, 50), border)
    # 테두리 안쪽의 금줄
    gold = rrect_mask(w, h, (8 * s, 8 * s, w - 8 * s, h - 8 * s), 14 * s // 2)
    img = paint(img, solid(w, h, INK), gold)
    gold_in = rrect_mask(w, h, (9 * s, 9 * s, w - 9 * s, h - 9 * s), 13 * s // 2)
    img = paint(img, gradient(w, h, GOLD_TOP, GOLD_BOTTOM), gold_in)
    panel = rrect_mask(w, h, (11 * s, 11 * s, w - 11 * s, h - 11 * s), 11 * s // 2)
    img = paint(img, gradient(w, h, pal["deep_top"], pal["deep_bottom"]), panel)

    # 배경의 결: 비스듬한 엷은 줄
    stripes = mask(w, h)
    d = ImageDraw.Draw(stripes)
    for i in range(-h, w + h, 14 * s):
        d.line([(i, 0), (i + h, h)], fill=22, width=5 * s)
    img = paint(img, solid(w, h, pal["light"]), ImageChops.multiply(stripes, panel))

    # 그림이 놓일 둥근 자리와 그 뒤의 빛
    mx, my, mr = CARD_MEDAL
    cx, cy, r = mx * s, py(my, scale=s), mr * s
    glow = radial(w, h, (cx, cy), r * 1.9, (*pal["light"], 120), (*pal["light"], 0), 1.4)
    img = paint(img, glow, panel)
    rays = mask(w, h)
    d = ImageDraw.Draw(rays)
    for k in range(16):
        a0 = math.radians(k * 22.5)
        a1 = math.radians(k * 22.5 + 8)
        d.polygon([(cx, cy), (cx + math.cos(a0) * r * 2.2, cy + math.sin(a0) * r * 2.2),
                   (cx + math.cos(a1) * r * 2.2, cy + math.sin(a1) * r * 2.2)], fill=40)
    img = paint(img, solid(w, h, (255, 255, 255)), ImageChops.multiply(blur(rays, 3 * s), panel))
    ring = ellipse_mask(w, h, (cx - r - 3 * s, cy - r - 3 * s, cx + r + 3 * s, cy + r + 3 * s))
    img = paint(img, solid(w, h, INK), grow(ring, s))
    img = paint(img, gradient(w, h, GOLD_TOP, GOLD_BOTTOM, 60), ring)
    well = ellipse_mask(w, h, (cx - r, cy - r, cx + r, cy + r))
    img = paint(img, solid(w, h, INK), grow(well, s // 2))
    img = paint(img, radial(w, h, (cx, cy - r * 0.3), r * 1.3, pal["mid"], pal["dark"]), well)

    # 위의 띠(HELP / CURSE 가 얹힌다)
    ry = py(CARD_RIBBON_Y, scale=s)
    rh = 8 * s
    ribbon = poly_mask(w, h, [
        (22 * s, ry - rh), (w - 22 * s, ry - rh), (w - 16 * s, ry), (w - 22 * s, ry + rh),
        (22 * s, ry + rh), (16 * s, ry),
    ])
    img = paint(img, solid(w, h, INK), grow(ribbon, s))
    img = paint(img, gradient(w, h, pal["mid"], pal["dark"]), ribbon)
    img = paint(img, sheen(w, h, ribbon, 70, 90), ribbon)

    # 이름 판
    ny = py(CARD_NAME_Y, scale=s)
    nh = CARD_NAME_H * s / 2
    plate = poly_mask(w, h, [
        (12 * s, ny - nh), (w - 12 * s, ny - nh), (w - 6 * s, ny), (w - 12 * s, ny + nh),
        (12 * s, ny + nh), (6 * s, ny),
    ])
    img = paint(img, solid(w, h, INK), grow(plate, s))
    img = paint(img, gradient(w, h, GOLD_TOP, GOLD_BOTTOM), ImageChops.subtract(plate, shrink(plate, s)))
    img = paint(img, gradient(w, h, pal["dark"], pal["deep_bottom"]), shrink(plate, s))

    # 설명 자리: 살짝 어두운 판
    ty0, ty1 = py(CARD_TEXT_TOP, scale=s), py(CARD_TEXT_BOTTOM, scale=s)
    text = rrect_mask(w, h, (16 * s, ty0, w - 16 * s, ty1), 6 * s)
    img = paint(img, solid(w, h, (0, 0, 0, 130)), text)

    # 점수 알약
    pyy = py(CARD_POINTS_Y, scale=s)
    pill = rrect_mask(w, h, (w / 2 - 24 * s, pyy - 8 * s, w / 2 + 24 * s, pyy + 8 * s), 8 * s)
    img = paint(img, solid(w, h, INK), grow(pill, s))
    img = paint(img, gradient(w, h, pal["mid"], pal["dark"]), pill)
    img = paint(img, solid(w, h, (8, 8, 14)), shrink(pill, 2 * s))

    # 네 귀의 보석
    # 위쪽 보석은 띠에 겹치지 않게 띠 아래에 둔다.
    for gx, gy in ((19, 19), (CARD_W - 19, 19), (19, CARD_H - 34), (CARD_W - 19, CARD_H - 34)):
        gx_, gy_ = gx * s, py(gy, scale=s)
        gem = poly_mask(w, h, [(gx_, gy_ - 4 * s), (gx_ + 4 * s, gy_), (gx_, gy_ + 4 * s), (gx_ - 4 * s, gy_)])
        img = paint(img, solid(w, h, INK), grow(gem, s))
        img = paint(img, gradient(w, h, GOLD_TOP, GOLD_BOTTOM), gem)

    return img.resize((CARD_W * UHD, CARD_H * UHD), Image.LANCZOS)


def card_back():
    s = UHD * 2
    w, h = CARD_W * s, CARD_H * s
    img = canvas(w, h)
    outer = rrect_mask(w, h, (0, 0, w - 1, h - 1), 22 * s // 2)
    img = paint(img, solid(w, h, INK), outer)
    border = rrect_mask(w, h, (4 * s, 4 * s, w - 4 * s, h - 4 * s), 18 * s // 2)
    img = paint(img, gradient(w, h, GOLD_TOP, GOLD_BOTTOM, 70), border)
    panel = rrect_mask(w, h, (9 * s, 9 * s, w - 9 * s, h - 9 * s), 13 * s // 2)
    img = paint(img, solid(w, h, INK), grow(panel, s))
    img = paint(img, gradient(w, h, (40, 48, 120), (12, 14, 46)), panel)

    lattice = mask(w, h)
    d = ImageDraw.Draw(lattice)
    step = 16 * s
    for i in range(-h, w + h, step):
        d.line([(i, 0), (i + h, h)], fill=50, width=2 * s)
        d.line([(i, h), (i + h, 0)], fill=50, width=2 * s)
    img = paint(img, solid(w, h, (140, 160, 255)), ImageChops.multiply(lattice, panel))

    cx, cy = w / 2, h / 2
    glow = radial(w, h, (cx, cy), 62 * s, (120, 150, 255, 150), (120, 150, 255, 0), 1.3)
    img = paint(img, glow, panel)
    diamond = poly_mask(w, h, [(cx, cy - 46 * s), (cx + 32 * s, cy), (cx, cy + 46 * s), (cx - 32 * s, cy)])
    img = paint(img, solid(w, h, INK), grow(diamond, 2 * s))
    img = paint(img, gradient(w, h, GOLD_TOP, GOLD_BOTTOM), diamond)
    inner = poly_mask(w, h, [(cx, cy - 36 * s), (cx + 24 * s, cy), (cx, cy + 36 * s), (cx - 24 * s, cy)])
    img = paint(img, gradient(w, h, (70, 80, 190), (20, 24, 80)), inner)
    star = poly_mask(w, h, star_points(cx, cy + 2 * s, 18 * s, 7.5 * s))
    img = paint(img, solid(w, h, INK), grow(star, s))
    img = paint(img, gradient(w, h, (255, 255, 255), (255, 220, 120)), star)

    inset = ImageChops.subtract(
        rrect_mask(w, h, (16 * s, 16 * s, w - 16 * s, h - 16 * s), 8 * s),
        rrect_mask(w, h, (17 * s, 17 * s, w - 17 * s, h - 17 * s), 7 * s),
    )
    img = paint(img, gradient(w, h, GOLD_TOP, GOLD_BOTTOM), inset)
    return img.resize((CARD_W * UHD, CARD_H * UHD), Image.LANCZOS)


# ---------------------------------------------------------------- HUD 와 그 밖

def hud_slot():
    # 흰색으로 그려 게임에서 카드 색으로 물들인다.
    size = 24 * UHD * SS
    img = canvas(size, size)
    m = rrect_mask(size, size, (0, 0, size - 1, size - 1), 6 * UHD * SS)
    img = paint(img, solid(size, size, INK), m)
    ring = ImageChops.subtract(
        rrect_mask(size, size, (2 * UHD * SS, 2 * UHD * SS, size - 2 * UHD * SS, size - 2 * UHD * SS), 5 * UHD * SS),
        rrect_mask(size, size, (4 * UHD * SS, 4 * UHD * SS, size - 4 * UHD * SS, size - 4 * UHD * SS), 3 * UHD * SS),
    )
    img = paint(img, solid(size, size, (255, 255, 255)), ring)
    inner = rrect_mask(size, size, (4 * UHD * SS, 4 * UHD * SS, size - 4 * UHD * SS, size - 4 * UHD * SS), 3 * UHD * SS)
    img = paint(img, gradient(size, size, (110, 110, 110), (40, 40, 40)), inner)
    return finish(img, 24 * UHD)


def hud_tick():
    size = 12 * UHD * SS
    img = canvas(size, size)
    c = size / 2
    d = poly_mask(size, size, [(c, 0), (size, c), (c, size), (0, c)])
    img = paint(img, solid(size, size, INK), d)
    inner = poly_mask(size, size, [(c, size * 0.2), (size * 0.8, c), (c, size * 0.8), (size * 0.2, c)])
    img = paint(img, solid(size, size, (255, 255, 255)), inner)
    return finish(img, 12 * UHD)


def glow():
    size = 64 * UHD
    return radial(size, size, (size / 2, size / 2), size / 2, (255, 255, 255, 255), (255, 255, 255, 0), 0.8)


def menu_icon():
    size = 48 * UHD * SS
    img = canvas(size, size)
    s = size / 768

    def card(angle, kind, dx):
        layer = canvas(size, size)
        cw, ch = 300 * s, 420 * s
        box = (size / 2 - cw / 2 + dx * s, size / 2 - ch / 2 + 20 * s, size / 2 + cw / 2 + dx * s, size / 2 + ch / 2 + 20 * s)
        m = rrect_mask(size, size, box, 46 * s)
        pal = KINDS[kind]
        layer = paint(layer, solid(size, size, INK), grow(m, int(24 * s)))
        layer = paint(layer, gradient(size, size, pal["light"], pal["dark"]), m)
        inner = rrect_mask(size, size, (box[0] + 34 * s, box[1] + 34 * s, box[2] - 34 * s, box[3] - 34 * s), 22 * s)
        layer = paint(layer, gradient(size, size, pal["deep_top"], pal["deep_bottom"]), inner)
        return layer.rotate(angle, resample=Image.BICUBIC, center=(size / 2, size * 0.75))

    img = Image.alpha_composite(img, card(18, "curse", -40))
    img = Image.alpha_composite(img, card(-14, "help", 40))
    star = poly_mask(size, size, star_points(size / 2 + 30 * s, size / 2 + 10 * s, 170 * s, 72 * s))
    img = paint(img, solid(size, size, INK), grow(star, int(26 * s)))
    img = paint(img, gradient(size, size, (255, 245, 170), (255, 170, 30)), star)
    img = paint(img, sheen(size, size, star, 90), star)
    return finish(img, 48 * UHD)


def refresh_icon():
    size = 32 * UHD * SS
    img = canvas(size, size)
    c = size / 2
    r = size * 0.30
    width = size * 0.13
    arc = mask(size, size)
    # PIL 의 각도는 3시 방향에서 시계 방향으로 잰다. 3시 위쪽에 틈을 두고 그 끝에 화살촉.
    ImageDraw.Draw(arc).arc((c - r, c - r, c + r, c + r), 20, 320, fill=255, width=int(width))
    theta = math.radians(320)
    end = (c + math.cos(theta) * (r - width / 2), c + math.sin(theta) * (r - width / 2))
    tangent = (-math.sin(theta), math.cos(theta))
    normal = (math.cos(theta), math.sin(theta))
    tip = (end[0] + tangent[0] * size * 0.20, end[1] + tangent[1] * size * 0.20)
    wing = size * 0.17
    head = poly_mask(size, size, [
        tip,
        (end[0] + normal[0] * wing, end[1] + normal[1] * wing),
        (end[0] - normal[0] * wing, end[1] - normal[1] * wing),
    ])
    shape = ImageChops.lighter(arc, head)
    img = paint(img, solid(size, size, INK), grow(shape, size * 0.035))
    img = paint(img, gradient(size, size, (255, 255, 255), (200, 220, 240)), shape)
    return finish(img, 32 * UHD)


def badge():
    # 메뉴 단추 위의 알림 점. 오늘 아직 아무 난이도도 깨지 않았을 때 뜬다.
    size = 16 * UHD * SS
    img = canvas(size, size)
    c = size / 2
    dot = ellipse_mask(size, size, (size * 0.06, size * 0.06, size * 0.94, size * 0.94))
    img = paint(img, solid(size, size, INK), dot)
    inner = ellipse_mask(size, size, (size * 0.14, size * 0.14, size * 0.86, size * 0.86))
    img = paint(img, gradient(size, size, (255, 110, 110), (210, 20, 40)), inner)
    img = paint(img, sheen(size, size, inner, 90), inner)
    bar = rrect_mask(size, size, (c - size * 0.08, size * 0.24, c + size * 0.08, size * 0.60), size * 0.06)
    dot2 = ellipse_mask(size, size, (c - size * 0.09, size * 0.66, c + size * 0.09, size * 0.84))
    img = paint(img, solid(size, size, (255, 255, 255)), ImageChops.lighter(bar, dot2))
    return finish(img, 16 * UHD)


def ring_featured():
    # 피처드 레벨의 금테. 난이도 얼굴 뒤에 깔리고, 얼굴보다 조금 커서 테만 보인다.
    size = 64 * UHD * SS
    img = canvas(size, size)
    c = size / 2
    outer = ellipse_mask(size, size, (c - size * 0.47, c - size * 0.47, c + size * 0.47, c + size * 0.47))
    inner = ellipse_mask(size, size, (c - size * 0.37, c - size * 0.37, c + size * 0.37, c + size * 0.37))
    band = ImageChops.subtract(outer, inner)
    img = paint(img, solid(size, size, INK), grow(band, 3 * SS))
    img = paint(img, gradient(size, size, (255, 240, 150), (215, 135, 20), 70), band)
    img = paint(img, sheen(size, size, band, 120, 45), band)
    # 테 위의 반짝임 네 점
    for ang in (-60, 30, 120, 210):
        a = math.radians(ang)
        x = c + math.cos(a) * size * 0.42
        y = c + math.sin(a) * size * 0.42
        star = poly_mask(size, size, star_points(x, y, size * 0.05, size * 0.018, 4, -90))
        img = paint(img, solid(size, size, (255, 255, 240)), star)
    return finish(img, 64 * UHD)


def ring_flames():
    # 에픽 이상의 불꽃 테. 흰색으로 그려 게임에서 등급 색(주황, 하늘, 보라)으로 물들인다.
    # 불꽃은 위로 솟으므로 윗부분 불꽃을 크게, 끝을 위쪽으로 휘게 그린다.
    size = 80 * UHD * SS
    img = canvas(size, size)
    c = size / 2
    r_in = size * 0.27
    r_out = size * 0.34
    flames = mask(size, size)
    count = 13
    for k in range(count):
        a = math.radians(k * 360 / count - 90)
        dx, dy = math.cos(a), math.sin(a)
        up = max(0.0, -dy)                      # 위를 볼수록 1
        length = size * (0.045 + 0.095 * up ** 1.3)
        width = math.radians(360 / count * 0.62)
        base_l = (c + math.cos(a - width) * r_out * 0.92, c + math.sin(a - width) * r_out * 0.92)
        base_r = (c + math.cos(a + width) * r_out * 0.92, c + math.sin(a + width) * r_out * 0.92)
        tip = (c + dx * (r_out + length), c + dy * (r_out + length) - length * 0.45 * (1 - up))
        # 끝으로 갈수록 가늘어지는 혀 모양
        bend = 0.55
        left = bezier([base_l, (base_l[0] + (tip[0] - base_l[0]) * bend + dy * size * 0.02,
                                base_l[1] + (tip[1] - base_l[1]) * bend - dx * size * 0.02), tip], 16)
        right = bezier([tip, (base_r[0] + (tip[0] - base_r[0]) * bend - dy * size * 0.02,
                              base_r[1] + (tip[1] - base_r[1]) * bend + dx * size * 0.02), base_r], 16)
        ImageDraw.Draw(flames).polygon(left + right[1:], fill=255)
    ring = ImageChops.subtract(
        ellipse_mask(size, size, (c - r_out, c - r_out, c + r_out, c + r_out)),
        ellipse_mask(size, size, (c - r_in, c - r_in, c + r_in, c + r_in)),
    )
    shape = ImageChops.lighter(blur(flames, 3 * SS).point(lambda v: 255 if v > 100 else 0), ring)
    img = paint(img, solid(size, size, INK), grow(shape, 3 * SS))
    # 안쪽(뿌리)이 밝고 끝이 어두운 빛. 물들이면 뿌리는 밝은 색, 끝은 짙은 색이 된다.
    img = paint(img, radial(size, size, (c, c), size * 0.5, (255, 255, 255), (150, 150, 150), 1.6), shape)
    core = ImageChops.multiply(shape, ellipse_mask(size, size, (c - r_out * 1.08, c - r_out * 1.08, c + r_out * 1.08, c + r_out * 1.08)))
    img = paint(img, solid(size, size, (255, 255, 255, 140)), core)
    return finish(img, 80 * UHD)


def main():
    os.makedirs(OUT, exist_ok=True)
    for kind in KINDS:
        save(card_face(kind), f"card-{kind}.png")
    save(card_back(), "card-back.png")
    for name, draw in ICONS.items():
        save(draw(), f"icon-{name}.png")
    save(hud_slot(), "hud-slot.png")
    save(hud_tick(), "hud-tick.png")
    save(glow(), "glow.png")
    save(menu_icon(), "menu-icon.png")
    save(refresh_icon(), "refresh-icon.png")
    save(badge(), "badge.png")
    save(ring_featured(), "ring-featured.png")
    save(ring_flames(), "ring-flames.png")


if __name__ == "__main__":
    main()
