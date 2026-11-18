"""Makes the SceneDemo project's assets (Projects/SceneDemo/Assets): its own models (a cube, a sphere, a cylinder, a
ground and a poster as OBJ) and checker texture, and the CC0 image of its poster copied from UICanvas'. Run it after
changing what it makes:
python Tools/GenerateSceneDemo.py
The .meta files are the editor's (it writes them on the project's first open; they are kept with the files).
"""

import math
import os
import shutil
import struct
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = os.path.join(ROOT, "Projects", "SceneDemo", "Assets")


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
    on = ((x // 16) + (y // 16)) % 2 == 0
    return (214, 218, 222, 255) if on else (150, 156, 164, 255)


def obj(name, comment, vertices, uvs, normals, faces):
    """An OBJ of one object; faces are lists of (v, vt, vn) 1-based triples."""
    lines = [f"# {comment} (own).", f"o {name}"]
    lines += [f"v {x:.5f} {y:.5f} {z:.5f}" for x, y, z in vertices]
    lines += [f"vt {u:.5f} {v:.5f}" for u, v in uvs]
    lines += [f"vn {x:.5f} {y:.5f} {z:.5f}" for x, y, z in normals]
    for f in faces:
        lines.append("f " + " ".join(f"{a}/{b}/{c}" for a, b, c in f))
    return "\n".join(lines) + "\n"


def cube():
    v, vt, vn, f = [], [(0, 0), (1, 0), (1, 1), (0, 1)], [], []
    # Each face: its normal and two axes along it.
    for n, a, b in [((1, 0, 0), (0, 0, 1), (0, 1, 0)), ((-1, 0, 0), (0, 0, -1), (0, 1, 0)), ((0, 1, 0), (1, 0, 0), (0, 0, 1)),
                    ((0, -1, 0), (1, 0, 0), (0, 0, -1)), ((0, 0, 1), (-1, 0, 0), (0, 1, 0)), ((0, 0, -1), (1, 0, 0), (0, 1, 0))]:
        vn.append(n)
        base = len(v)
        for su, sv in [(-1, -1), (1, -1), (1, 1), (-1, 1)]:
            v.append(tuple(0.5 * n[i] + 0.5 * su * a[i] + 0.5 * sv * b[i] for i in range(3)))
        k = len(vn)
        f.append([(base + 1, 1, k), (base + 4, 4, k), (base + 3, 3, k), (base + 2, 2, k)])
    return obj("Cube", "A unit cube", v, vt, vn, f)


def sphere(rings=16, segments=24):
    v, vt, vn, f = [], [], [], []
    for r in range(rings + 1):
        theta = math.pi * r / rings
        for s in range(segments + 1):
            phi = 2 * math.pi * s / segments
            n = (math.sin(theta) * math.cos(phi), math.cos(theta), math.sin(theta) * math.sin(phi))
            v.append(tuple(0.5 * c for c in n))
            vn.append(n)
            vt.append((s / segments, 1 - r / rings))
    row = segments + 1
    for r in range(rings):
        for s in range(segments):
            a, b, c, d = r * row + s + 1, r * row + s + 2, (r + 1) * row + s + 2, (r + 1) * row + s + 1
            f.append([(a, a, a), (b, b, b), (c, c, c), (d, d, d)])
    return obj("Sphere", "A sphere of diameter 1", v, vt, vn, f)


def cylinder(segments=24):
    v, vt, vn, f = [], [], [], []
    # The side: two rings with outward normals.
    for s in range(segments + 1):
        phi = 2 * math.pi * s / segments
        n = (math.cos(phi), 0, math.sin(phi))
        for y in (-0.5, 0.5):
            v.append((0.5 * n[0], y, 0.5 * n[2]))
            vn.append(n)
            vt.append((s / segments, y + 0.5))
    for s in range(segments):
        a, b, c, d = 2 * s + 1, 2 * s + 3, 2 * s + 4, 2 * s + 2
        f.append([(a, a, a), (d, d, d), (c, c, c), (b, b, b)])
    # The caps: a fan each.
    for y, ny in ((0.5, 1), (-0.5, -1)):
        center = len(v) + 1
        v.append((0, y, 0))
        vn.append((0, ny, 0))
        vt.append((0.5, 0.5))
        first = len(v) + 1
        for s in range(segments + 1):
            phi = 2 * math.pi * s / segments
            v.append((0.5 * math.cos(phi), y, 0.5 * math.sin(phi)))
            vn.append((0, ny, 0))
            vt.append((0.5 + 0.5 * math.cos(phi), 0.5 + 0.5 * math.sin(phi)))
        for s in range(segments):
            a, b = first + s, first + s + 1
            tri = [(center, center, center), (b, b, b), (a, a, a)] if ny > 0 else [(center, center, center), (a, a, a), (b, b, b)]
            f.append(tri)
    return obj("Cylinder", "A cylinder of diameter 1 and height 1", v, vt, vn, f)


def plane(name, comment, size, tiles, facing_z=False):
    """A square of the size: on XZ facing up (the ground), or on XY facing -Z (a poster)."""
    h = size / 2
    if facing_z:
        v = [(-h, -h, 0), (h, -h, 0), (h, h, 0), (-h, h, 0)]
        n = (0, 0, -1)
    else:
        v = [(-h, 0, -h), (-h, 0, h), (h, 0, h), (h, 0, -h)]
        n = (0, 1, 0)
    vt = [(0, 0), (tiles, 0), (tiles, tiles), (0, tiles)] if facing_z else [(0, 0), (0, tiles), (tiles, tiles), (tiles, 0)]
    order = [1, 4, 3, 2] if facing_z else [1, 2, 3, 4]
    return obj(name, comment, v, vt, [n], [[(i, i, 1) for i in order]])


def main():
    write_text("Models/Cube.obj", cube())
    write_text("Models/Sphere.obj", sphere())
    write_text("Models/Cylinder.obj", cylinder())
    write_text("Models/Ground.obj", plane("Ground", "A 12 by 12 ground of 6 by 6 checker tiles", 12, 6))
    write_text("Models/Poster.obj", plane("Poster", "A 1 by 1 poster facing -Z", 1, 1, True))
    write_bytes("Textures/Checker.png", png(64, 64, checker))
    cc0 = os.path.join(ROOT, "Samples", "UICanvas", "Data", "Images", "CC0", "KenneyPanda.png")
    os.makedirs(os.path.join(ASSETS, "Textures", "CC0"), exist_ok=True)
    shutil.copyfile(cc0, os.path.join(ASSETS, "Textures", "CC0", "KenneyPanda.png"))
    print("SceneDemo assets written.")


if __name__ == "__main__":
    main()
