#!/usr/bin/env python3
"""
Deterministic generator for WiGLE 3DS Wardriver (3ds_wigle) packaging assets:
- meta/icon.png (48x48 RGBA PNG: Radar Scope with Concentric Rings, Sweep Beam & AP Blips)
- meta/banner.png (256x128 RGBA PNG: Radar Scope + "WIGLE 3DS WARDRIVER" title text)
- meta/audio.wav & meta/banner.bnr
"""

import math
import os
import struct
import wave
import zlib
from generate_bnr_shared import draw_banner_text_centered, write_cbmd_bnr  # type: ignore


def write_png(path: str, width: int, height: int, rgba_bytes: bytes) -> None:
    def chunk(tag: bytes, data: bytes) -> bytes:
        return (
            struct.pack(">I", len(data))
            + tag
            + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        )

    raw_rows = bytearray()
    stride = width * 4
    for y in range(height):
        raw_rows.append(0)
        raw_rows.extend(rgba_bytes[y * stride : (y + 1) * stride])

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    compressed = zlib.compress(bytes(raw_rows), 9)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"IDAT", compressed))
        f.write(chunk(b"IEND", b""))


def generate_icon(path: str) -> None:
    w, h = 48, 48
    buf = bytearray(w * h * 4)
    cx, cy = 23.5, 23.5
    for y in range(h):
        for x in range(w):
            idx = (y * w + x) * 4
            # Neon radar green border
            if x < 2 or x >= w - 2 or y < 2 or y >= h - 2:
                buf[idx : idx + 4] = bytes((70, 245, 130, 255))
                continue
            buf[idx : idx + 4] = bytes((8, 20, 14, 255))

            dx = x - cx
            dy = y - cy
            r = math.sqrt(dx * dx + dy * dy)

            # Radar blips (discovered Wi-Fi APs)
            if (x - 31) ** 2 + (y - 15) ** 2 <= 4 or (x - 16) ** 2 + (y - 30) ** 2 <= 4:
                buf[idx : idx + 4] = bytes((255, 235, 80, 255))
                continue

            # Concentric radar rings at r = 18, 12, 6 + crosshair axes
            if abs(r - 18.0) <= 0.9 or abs(r - 12.0) <= 0.8 or abs(r - 6.0) <= 0.8:
                buf[idx : idx + 4] = bytes((50, 220, 115, 255))
            elif r <= 18.0 and (abs(dx) <= 0.6 or abs(dy) <= 0.6):
                buf[idx : idx + 4] = bytes((35, 140, 75, 255))
            elif r <= 18.0 and dx > 0 and dy < 0 and abs(dx + dy) <= 2.2:
                # Diagonal radar sweep beam in top-right quadrant
                buf[idx : idx + 4] = bytes((120, 255, 175, 255))

    write_png(path, w, h, bytes(buf))


def generate_banner(path: str) -> bytes:
    w, h = 256, 128
    buf = bytearray(w * h * 4)
    cx, cy = 128.0, 44.0
    for y in range(h):
        for x in range(w):
            idx = (y * w + x) * 4
            r_col = 8 + (y * 10) // h
            g_col = 22 + (y * 24) // h
            b_col = 16 + (y * 18) // h
            buf[idx : idx + 4] = bytes((r_col, g_col, b_col, 255))

            dx = x - cx
            dy = y - cy
            dist = math.sqrt(dx * dx + dy * dy)
            if dist <= 30.0:
                if abs(dist - 29.0) <= 1.2 or abs(dist - 19.0) <= 1.0 or abs(dist - 9.0) <= 1.0:
                    buf[idx : idx + 4] = bytes((70, 245, 140, 255))
                elif dx > 0 and dy < 0 and abs(dx + dy) <= 3.0:
                    buf[idx : idx + 4] = bytes((150, 255, 195, 255))

    draw_banner_text_centered(buf, w, h, 86, "WIGLE 3DS", 2, (95, 255, 155, 255))
    draw_banner_text_centered(buf, w, h, 108, "WIFI WARDRIVER & RADAR", 1, (215, 245, 225, 255))
    raw = bytes(buf)
    write_png(path, w, h, raw)
    return raw


def generate_audio(path: str) -> None:
    sample_rate = 22050
    duration_s = 0.8
    n_samples = int(sample_rate * duration_s)
    with wave.open(path, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        frames = bytearray()
        for i in range(n_samples):
            t = i / sample_rate
            freq = 880.0 if t < 0.2 else (1174.66 if t < 0.4 else 1318.51)
            env = max(0.0, 1.0 - (t / duration_s))
            val = int(11000 * env * math.sin(2.0 * math.pi * freq * t))
            frames.extend(struct.pack("<h", val))
        wf.writeframes(bytes(frames))


def main() -> None:
    meta_dir = os.path.dirname(os.path.abspath(__file__))
    generate_icon(os.path.join(meta_dir, "icon.png"))
    banner_rgba = generate_banner(os.path.join(meta_dir, "banner.png"))
    generate_audio(os.path.join(meta_dir, "audio.wav"))
    write_cbmd_bnr(os.path.join(meta_dir, "banner.bnr"), (880.0, 1174.66, 1318.51), banner_rgba)
    print("Generated WiGLE 3DS meta/icon.png, meta/banner.png, meta/audio.wav, meta/banner.bnr")


if __name__ == "__main__":
    main()
