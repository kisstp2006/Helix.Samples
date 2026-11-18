"""Puts the CC0 art the samples show into their data folders, from the downloaded packs (see each sample's
CREDITS.txt for where they come from). Images are taken as they are when they fit in 256 pixels (the UI's atlas
takes them then); larger ones are shrunk by averaging. Python's standard library only.

    python Tools/PrepareImages.py --ui kenney_ui-pack.zip --icons Kenney_gameIcons.zip
        --animals kenney_animalPackRedux.zip --landscape alps_field.png

Then run Tools/GenerateData.py, which writes the images' .meta and lists them.
"""

import argparse
import os
import struct
import zipfile
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SAMPLES = os.path.join(ROOT, "Samples")
MAX_SIDE = 256

# What goes where: the pack (an argument), the file in it ("" for the argument itself), the sample and its path.
IMAGES = [
    ("ui", "PNG/Blue/Default/button_rectangle_depth_gradient.png", "UIRender", "Images/CC0/KenneyButtonBlue.png"),
    ("ui", "PNG/Green/Default/button_rectangle_depth_gradient.png", "UIRender", "Images/CC0/KenneyButtonGreen.png"),
    ("ui", "PNG/Grey/Default/button_square_depth_flat.png", "UIRender", "Images/CC0/KenneyPanel.png"),
    ("ui", "PNG/Yellow/Default/star.png", "UIRender", "Images/CC0/KenneyStar.png"),
    ("icons", "PNG/White/2x/star.png", "UIRender", "Images/CC0/KenneyIconStar.png"),
    ("animals", "PNG/Round/panda.png", "UIRender", "Images/CC0/KenneyPanda.png"),
    ("animals", "PNG/Round/penguin.png", "UIRender", "Images/CC0/KenneyPenguin.png"),
    ("landscape", "", "UIRender", "Images/CC0/PolyHavenAlpsField.png"),
    ("animals", "PNG/Round/panda.png", "UIImmediate", "Images/CC0/KenneyPanda.png"),
    ("animals", "PNG/Round/penguin.png", "UIImmediate", "Images/CC0/KenneyPenguin.png"),
    ("animals", "PNG/Round/owl.png", "UIImmediate", "Images/CC0/KenneyOwl.png"),
    ("animals", "PNG/Round/rabbit.png", "UIImmediate", "Images/CC0/KenneyRabbit.png"),
    ("animals", "PNG/Round/narwhal.png", "UIImmediate", "Images/CC0/KenneyNarwhal.png"),
    ("animals", "PNG/Round/frog.png", "UIImmediate", "Images/CC0/KenneyFrog.png"),
    ("icons", "PNG/White/2x/gear.png", "UIImmediate", "Images/CC0/KenneyIconGear.png"),
    ("icons", "PNG/White/2x/trophy.png", "UIImmediate", "Images/CC0/KenneyIconTrophy.png"),
    ("icons", "PNG/White/2x/medal1.png", "UIImmediate", "Images/CC0/KenneyIconMedal.png"),
    ("icons", "PNG/White/2x/audioOn.png", "UIImmediate", "Images/CC0/KenneyIconSound.png"),
    ("icons", "PNG/White/2x/musicOn.png", "UIImmediate", "Images/CC0/KenneyIconMusic.png"),
    ("icons", "PNG/White/2x/wrench.png", "UIImmediate", "Images/CC0/KenneyIconWrench.png"),
    ("icons", "PNG/White/2x/gear.png", "UICanvas", "Images/CC0/KenneyIconGear.png"),
    ("icons", "PNG/White/2x/trophy.png", "UICanvas", "Images/CC0/KenneyIconTrophy.png"),
    ("icons", "PNG/White/2x/medal1.png", "UICanvas", "Images/CC0/KenneyIconMedal.png"),
    ("icons", "PNG/White/2x/audioOn.png", "UICanvas", "Images/CC0/KenneyIconSound.png"),
    ("icons", "PNG/White/2x/musicOn.png", "UICanvas", "Images/CC0/KenneyIconMusic.png"),
    ("icons", "PNG/White/2x/wrench.png", "UICanvas", "Images/CC0/KenneyIconWrench.png"),
    ("animals", "PNG/Round/panda.png", "UICanvas", "Images/CC0/KenneyPanda.png"),
    ("animals", "PNG/Round/penguin.png", "UICanvas", "Images/CC0/KenneyPenguin.png"),
    ("animals", "PNG/Round/owl.png", "UICanvas", "Images/CC0/KenneyOwl.png"),
    ("animals", "PNG/Round/rabbit.png", "UICanvas", "Images/CC0/KenneyRabbit.png"),
    ("animals", "PNG/Round/narwhal.png", "UICanvas", "Images/CC0/KenneyNarwhal.png"),
    ("animals", "PNG/Round/frog.png", "UICanvas", "Images/CC0/KenneyFrog.png"),
]


def chunks(data):
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
    i = 8
    while i < len(data):
        length, = struct.unpack(">I", data[i:i + 4])
        yield data[i + 4:i + 8], data[i + 8:i + 8 + length]
        i += 12 + length


def size_of(data):
    for kind, body in chunks(data):
        if kind == b"IHDR":
            return struct.unpack(">II", body[:8])
    raise ValueError("no IHDR")


def decode_rgb(data):
    """8 bit RGB or RGBA without interlacing, as rows of RGBA."""
    idat = b""
    for kind, body in chunks(data):
        if kind == b"IHDR":
            width, height, depth, color, _, _, interlace = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            idat += body
    if depth != 8 or color not in (2, 6) or interlace:
        raise ValueError("only 8 bit RGB or RGBA without interlacing is shrunk here")
    channels = 3 if color == 2 else 4
    raw = zlib.decompress(idat)
    stride = width * channels
    prev = bytearray(stride)
    rows = []
    for y in range(height):
        kind = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for k in range(stride):
            a = line[k - channels] if k >= channels else 0
            b = prev[k]
            c = prev[k - channels] if k >= channels else 0
            if kind == 1:
                line[k] = (line[k] + a) & 255
            elif kind == 2:
                line[k] = (line[k] + b) & 255
            elif kind == 3:
                line[k] = (line[k] + ((a + b) >> 1)) & 255
            elif kind == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[k] = (line[k] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        prev = line
        rgba = bytearray()
        for x in range(width):
            px = line[x * channels:(x + 1) * channels]
            rgba += px + (b"\xff" if channels == 3 else b"")
        rows.append(rgba)
    return width, height, rows


def encode_rgba(width, height, rows):
    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)
    raw = b"".join(b"\x00" + bytes(r) for r in rows)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def shrink(data):
    """The image halved (averaging 2 by 2) until it fits."""
    width, height, rows = decode_rgb(data)
    while width > MAX_SIDE or height > MAX_SIDE:
        w, h = width // 2, height // 2
        out = []
        for y in range(h):
            row = bytearray()
            for x in range(w):
                for k in range(4):
                    s = (rows[2 * y][8 * x + k] + rows[2 * y][8 * x + 4 + k] + rows[2 * y + 1][8 * x + k] +
                         rows[2 * y + 1][8 * x + 4 + k])
                    row.append((s + 2) // 4)
            out.append(row)
        width, height, rows = w, h, out
    return encode_rgba(width, height, rows)


def main():
    parser = argparse.ArgumentParser(description="Puts the samples' CC0 art into their data folders.")
    for key in ("ui", "icons", "animals", "landscape"):
        parser.add_argument("--" + key, required=True)
    args = vars(parser.parse_args())
    archives = {}
    for pack, member, sample, path in IMAGES:
        if member:
            if pack not in archives:
                archives[pack] = zipfile.ZipFile(args[pack])
            data = archives[pack].read(member)
        else:
            with open(args[pack], "rb") as f:
                data = f.read()
        width, height = size_of(data)
        if width > MAX_SIDE or height > MAX_SIDE:
            data = shrink(data)
        full = os.path.join(SAMPLES, sample, "Data", path)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "wb") as f:
            f.write(data)
        print(f"{sample}/{path}: {size_of(data)[0]} x {size_of(data)[1]}")


if __name__ == "__main__":
    main()
