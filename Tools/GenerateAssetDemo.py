"""Draws the AssetDemo project's own assets (Projects/AssetDemo/Assets): textures, a pixel art sprite, models and
palettes (read by the project's scripted importer). Run it after changing what it draws:
python Tools/GenerateAssetDemo.py
The .meta files are the editor's (it writes them on the project's first open; they are kept with the files).
"""

import math
import os
import struct
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = os.path.join(ROOT, "Projects", "AssetDemo", "Assets")


def write_bytes(path, data):
    full = os.path.join(ASSETS, path)
    os.makedirs(os.path.dirname(full), exist_ok=True)
    with open(full, "wb") as f:
        f.write(data)


def write_text(path, text):
    write_bytes(path, text.encode("ascii"))


def png(width, height, pixel):
    """An RGBA PNG; pixel(x, y) gives (r, g, b, a) with y going down."""
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            rows.extend(bytes(pixel(x, y)))

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + chunk(b"IEND", b"")


def checker(x, y):
    on = ((x // 8) + (y // 8)) % 2 == 0
    return (232, 236, 240, 255) if on else (58, 96, 140, 255)


def rings(x, y):
    d = math.hypot(x - 31.5, y - 31.5)
    t = 0.5 + 0.5 * math.cos(d * 0.55)
    edge = max(0.0, min(1.0, (32.0 - d) / 2.0))
    return (int(250 * t + 40 * (1 - t)), int(150 * t + 30 * (1 - t)), int(60 + 150 * (1 - t)), int(255 * edge))


HEART = ["................",
         "...XX......XX...",
         "..XooX....XooX..",
         ".XowooX..XooooX.",
         ".XowoooXXoooooX.",
         ".XoooooooooooooX",
         ".XoooooooooooooX",
         "..XoooooooooooX.",
         "...XoooooooooX..",
         "....XoooooooX...",
         ".....XoooooX....",
         "......XoooX.....",
         ".......XoX......",
         "........X.......",
         "................",
         "................"]


def heart(x, y):
    c = HEART[y][x]
    return {"X": (60, 20, 30, 255), "o": (220, 50, 70, 255), "w": (255, 220, 225, 255)}.get(c, (0, 0, 0, 0))


CUBE = """# A unit cube (own).
o Cube
v -0.5 -0.5 -0.5
v 0.5 -0.5 -0.5
v 0.5 0.5 -0.5
v -0.5 0.5 -0.5
v -0.5 -0.5 0.5
v 0.5 -0.5 0.5
v 0.5 0.5 0.5
v -0.5 0.5 0.5
vn 0 0 -1
vn 0 0 1
vn -1 0 0
vn 1 0 0
vn 0 -1 0
vn 0 1 0
f 1//1 4//1 3//1 2//1
f 5//2 6//2 7//2 8//2
f 1//3 5//3 8//3 4//3
f 2//4 3//4 7//4 6//4
f 1//5 2//5 6//5 5//5
f 4//6 8//6 7//6 3//6
"""

PYRAMID = """# A square pyramid (own).
o Pyramid
v -0.5 0 -0.5
v 0.5 0 -0.5
v 0.5 0 0.5
v -0.5 0 0.5
v 0 0.8 0
f 1 2 3 4
f 1 5 2
f 2 5 3
f 3 5 4
f 4 5 1
"""

PALETTES = {"Sunset": ["#2b1b4b", "#6b2d6b", "#c2416b", "#f07a4a", "#ffc65c", "#fff1b5"],
            "Forest": ["#10261b", "#1f4a2c", "#3e7a3a", "#7fae4c", "#c9d986"]}


def main():
    write_bytes("Textures/Checker.png", png(64, 64, checker))
    write_bytes("Textures/Rings.png", png(64, 64, rings))
    write_bytes("Textures/Pixel/Heart.png", png(16, 16, heart))
    write_text("Models/Cube.obj", CUBE)
    write_text("Models/Pyramid.obj", PYRAMID)
    for name, colors in PALETTES.items():
        write_text(f"Data/{name}.palette", "# One color a line (the project's PaletteImporter).\n" + "\n".join(colors) + "\n")


if __name__ == "__main__":
    main()
