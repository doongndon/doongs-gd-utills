#!/usr/bin/env python3
"""assets/logo.svg 와 같은 도형을 336x336 PNG(logo.png)로 굽는다.

Geode 는 모드 패키지 안에 336x336 logo.png 를 요구하는데, 이 환경에는 SVG
래스터라이저가 없다. 그래서 도형을 거리 함수(SDF)로 직접 계산한다.
거리 함수는 "이 점이 도형 경계에서 얼마나 떨어져 있는가"를 알려주는 자로,
경계 근처 값을 0~1 로 부드럽게 깎아 주면 그게 곧 안티앨리어싱이다.
"""

import struct
import zlib

SIZE = 336
CENTER = SIZE / 2.0

BG_TOP = (0x12, 0x20, 0x1A)
BG_BOTTOM = (0x06, 0x0C, 0x09)
FG_TOP = (0x7C, 0xF5, 0xB4)
FG_BOTTOM = (0x2E, 0xBE, 0x6C)

CANVAS_RADIUS = 74.0
RING_HALF = 108.0
RING_RADIUS = 34.0
RING_WIDTH = 19.0
CORE_HALF = 38.0
CORE_RADIUS = 13.0


def rounded_rect_distance(dx, dy, half, radius):
    qx = abs(dx) - half + radius
    qy = abs(dy) - half + radius
    ox = qx if qx > 0.0 else 0.0
    oy = qy if qy > 0.0 else 0.0
    outside = (ox * ox + oy * oy) ** 0.5
    inside = min(max(qx, qy), 0.0)
    return outside + inside - radius


def coverage(value):
    return 0.0 if value <= -0.5 else (1.0 if value >= 0.5 else value + 0.5)


def mix(low, high, t):
    return tuple(int(round(a + (b - a) * t)) for a, b in zip(low, high))


def build_rows():
    rows = []
    for y in range(SIZE):
        dy = y + 0.5 - CENTER
        t = y / (SIZE - 1)
        background = mix(BG_TOP, BG_BOTTOM, t)
        foreground = mix(FG_TOP, FG_BOTTOM, t)

        row = bytearray()
        row.append(0)  # PNG filter: none
        for x in range(SIZE):
            dx = x + 0.5 - CENTER

            canvas = coverage(-rounded_rect_distance(dx, dy, CENTER, CANVAS_RADIUS))
            if canvas <= 0.0:
                row += b"\x00\x00\x00\x00"
                continue

            ring = rounded_rect_distance(dx, dy, RING_HALF, RING_RADIUS)
            shape = max(
                coverage(-(abs(ring) - RING_WIDTH / 2.0)),
                coverage(-rounded_rect_distance(dx, dy, CORE_HALF, CORE_RADIUS)),
            )

            pixel = mix(background, foreground, shape)
            row += bytes(pixel) + bytes((int(round(canvas * 255)),))
        rows.append(bytes(row))
    return b"".join(rows)


def chunk(tag, payload):
    body = tag + payload
    return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body))


def main():
    header = struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0)
    png = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", header)
        + chunk(b"IDAT", zlib.compress(build_rows(), 9))
        + chunk(b"IEND", b"")
    )
    with open("logo.png", "wb") as handle:
        handle.write(png)
    print(f"logo.png written ({len(png)} bytes)")


if __name__ == "__main__":
    main()
