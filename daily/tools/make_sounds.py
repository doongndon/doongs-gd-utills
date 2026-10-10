#!/usr/bin/env python3
"""Daily Challenge 모드의 효과음을 만든다.

카드가 깔리고 뒤집히고 골라지는 소리, 3-2-1 카운트다운과 GO, 방패가 깨지는 소리를
수식으로 합성해 daily/resources/ 에 ogg 로 넣는다. GD 의 효과음도 ogg 다.
ffmpeg(libvorbis)가 있어야 한다.

    python3 daily/tools/make_sounds.py
"""
import os
import subprocess
import tempfile
import wave

import numpy as np

OUT = os.path.join(os.path.dirname(__file__), "..", "resources")
RATE = 44100
# 같은 소리가 늘 같게 나오도록 씨앗을 고정한다.
RNG = np.random.default_rng(20261010)


def t(seconds):
    return np.arange(int(RATE * seconds)) / RATE


def env(length, attack, decay):
    """빠르게 올라갔다가 지수로 줄어드는 소리 크기."""
    x = t(length)
    up = np.clip(x / max(attack, 1e-4), 0, 1)
    return up * np.exp(-np.maximum(x - attack, 0) / decay)


def tone(freq, length, attack=0.004, decay=0.2, harmonics=((1, 1.0),), sweep=None):
    x = t(length)
    f = np.full_like(x, freq) if sweep is None else freq + (sweep - freq) * (x / length)
    phase = 2 * np.pi * np.cumsum(f) / RATE
    wave_ = sum(gain * np.sin(phase * k) for k, gain in harmonics)
    return wave_ * env(length, attack, decay)


def noise(length):
    return RNG.uniform(-1, 1, int(RATE * length))


def lowpass(x, cutoff):
    a = np.exp(-2 * np.pi * cutoff / RATE)
    y = np.zeros_like(x)
    acc = 0.0
    for i, v in enumerate(x):
        acc = (1 - a) * v + a * acc
        y[i] = acc
    return y


def highpass(x, cutoff):
    return x - lowpass(x, cutoff)


def mix(length, *parts):
    out = np.zeros(int(RATE * length))
    for start, part in parts:
        i = int(RATE * start)
        n = min(len(part), len(out) - i)
        out[i:i + n] += part[:n]
    return out


def fade_out(x, seconds=0.02):
    n = min(len(x), int(RATE * seconds))
    x = x.copy()
    x[-n:] *= np.linspace(1, 0, n)
    return x


def normalize(x, peak=0.8):
    m = np.max(np.abs(x))
    return x if m == 0 else x * (peak / m)


def deal():
    # 카드가 미끄러져 들어오는 '스윽'
    n = noise(0.26)
    band = highpass(lowpass(n, 3200), 500)
    x = t(0.26)
    shape = np.sin(np.pi * np.clip(x / 0.26, 0, 1)) ** 1.6
    return normalize(fade_out(band * shape), 0.55)


def flip():
    # 카드가 뒤집히는 '탁'
    snap = highpass(noise(0.03), 1800) * env(0.03, 0.001, 0.006)
    body = tone(210, 0.06, 0.001, 0.018)
    second = highpass(noise(0.02), 2600) * env(0.02, 0.001, 0.004)
    return normalize(fade_out(mix(0.09, (0, snap), (0, body * 0.6), (0.035, second * 0.6))), 0.7)


def pick():
    # 고르기: 맑게 울리는 두 음
    bell = ((1, 1.0), (2, 0.35), (3, 0.12))
    a = tone(1046.5, 0.7, 0.003, 0.22, bell)
    b = tone(1568.0, 0.7, 0.003, 0.26, bell)
    shimmer = tone(2093.0, 0.5, 0.01, 0.15) * 0.3
    sparkle = highpass(noise(0.12), 5000) * env(0.12, 0.002, 0.03) * 0.25
    return normalize(fade_out(mix(0.75, (0, a), (0.06, b), (0.06, shimmer), (0, sparkle))), 0.8)


def tick():
    # 카운트다운 3, 2, 1
    beep = tone(880, 0.16, 0.002, 0.05, ((1, 1.0), (2, 0.2)), sweep=760)
    click = highpass(noise(0.01), 3000) * env(0.01, 0.0005, 0.002) * 0.4
    return normalize(fade_out(mix(0.18, (0, beep), (0, click))), 0.7)


def go():
    # GO: 위로 오르는 화음
    bright = ((1, 1.0), (2, 0.4), (3, 0.15))
    notes = [523.25, 659.25, 783.99, 1046.5]
    parts = [(i * 0.055, tone(f, 0.8 - i * 0.055, 0.004, 0.28, bright) * (0.8 + 0.07 * i)) for i, f in enumerate(notes)]
    sparkle = highpass(noise(0.25), 6000) * env(0.25, 0.01, 0.08) * 0.18
    return normalize(fade_out(mix(0.85, *parts, (0.16, sparkle))), 0.85)


def shield():
    # 방패가 깨지는 소리: 유리 부서지는 쨍그랑
    crash = highpass(noise(0.45), 2200) * env(0.45, 0.001, 0.09)
    drop = tone(1900, 0.35, 0.001, 0.09, ((1, 1.0), (2, 0.3)), sweep=520) * 0.5
    tinkles = []
    for i in range(9):
        f = RNG.uniform(3200, 6800)
        start = RNG.uniform(0.0, 0.18)
        tinkles.append((start, tone(f, 0.25, 0.001, RNG.uniform(0.03, 0.07)) * RNG.uniform(0.15, 0.35)))
    thump = tone(130, 0.2, 0.002, 0.05) * 0.6
    return normalize(fade_out(mix(0.5, (0, crash), (0, drop), (0, thump), *tinkles)), 0.8)


def lose():
    # 죽어서 카드를 잃을 때: 낮게 떨어지는 두 음
    a = tone(392.0, 0.35, 0.004, 0.12, ((1, 1.0), (2, 0.25)), sweep=330)
    b = tone(293.7, 0.45, 0.004, 0.16, ((1, 1.0), (2, 0.25)), sweep=220)
    return normalize(fade_out(mix(0.6, (0, a), (0.12, b))), 0.6)


SOUNDS = {
    "sfx-deal": deal,
    "sfx-flip": flip,
    "sfx-pick": pick,
    "sfx-tick": tick,
    "sfx-go": go,
    "sfx-shield": shield,
    "sfx-lose": lose,
}


def write_ogg(samples, name):
    pcm = (np.clip(samples, -1, 1) * 32767).astype(np.int16)
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, name + ".wav")
        with wave.open(path, "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(RATE)
            w.writeframes(pcm.tobytes())
        out = os.path.join(OUT, name + ".ogg")
        subprocess.run(
            ["ffmpeg", "-y", "-loglevel", "error", "-i", path, "-c:a", "libvorbis", "-q:a", "5", out],
            check=True,
        )
    print("wrote", os.path.relpath(out), f"{len(samples) / RATE:.2f}s")


def main():
    os.makedirs(OUT, exist_ok=True)
    for name, make in SOUNDS.items():
        write_ogg(make(), name)


if __name__ == "__main__":
    main()
