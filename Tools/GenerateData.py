"""Writes what a build would leave in each sample's data folder (Samples/<name>/Data) beside the sample's own files:
settings.asset, the script assets (MonoScript) the scene names, the scene and assets.index, which lists every file of
the folder (not the .meta files, which go with theirs). The samples' own images are drawn here, and every image
gets its .meta (readable, so the UI puts it into its atlas). The sample's own files (markup, style sheets) are left
as they are. Run it after adding a file to a sample: python Tools/GenerateData.py
"""

import hashlib
import math
import os
import struct
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SAMPLES = os.path.join(ROOT, "Samples")
HEADER = "%YAML 1.1\n%TAG !h! tag:helixengine,2026:\n"

# The script classes the samples' scenes use: (asset name, module or assembly, namespace, class); the C++ UIView in
# every sample, a sample's own C# classes beside it.
SCRIPTS = [("UIView", "HelixUI", "HelixEngine.UI", "UIView")]
SAMPLE_SCRIPTS = {"UIElements": [("UIElementsDemo", "HelixSamples.UIElements", "HelixSamples", "UIElementsDemo")],
                  "UIControls": [("UIControlsDemo", "HelixSamples.UIControls", "HelixSamples", "UIControlsDemo"),
                                 ("UIControlsCpp", "HelixSamples", "HelixSamples", "UIControlsCpp")],
                  "UICollections": [("UICollectionsDemo", "HelixSamples.UICollections", "HelixSamples", "UICollectionsDemo")],
                  "UIBinding": [("UIBindingDemo", "HelixSamples.UIBinding", "HelixSamples", "UIBindingDemo")]}

# The fileID of the main object of a script asset (a MonoBehaviour's class ID times 100000).
MAIN_SCRIPT_OBJECT = 11400000


def guid(sample, path):
    return hashlib.md5(f"helix-samples:{sample}:{path}".encode()).hexdigest()


def write(folder, path, text):
    full = os.path.join(folder, path)
    os.makedirs(os.path.dirname(full), exist_ok=True)
    with open(full, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def script_asset(module, namespace, name):
    return (f"{HEADER}--- !h!115 &11500000\nMonoScript:\n  m_Name: {name}\n  m_ExecutionOrder: 0\n  m_ClassName: {name}\n"
            f"  m_Namespace: {namespace}\n  m_AssemblyName: {module}\n")


# ---------------------------------------------------------------------------------------------------------- images


def png(width, height, pixels):
    """An RGBA PNG of rows top first, each pixel four bytes."""
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    raw = b"".join(b"\x00" + bytes(pixels[y * width * 4:(y + 1) * width * 4]) for y in range(height))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def hex_color(text, alpha=1.0):
    return (int(text[1:3], 16) / 255, int(text[3:5], 16) / 255, int(text[5:7], 16) / 255, alpha)


def mix(a, b, t):
    t = max(0.0, min(1.0, t))
    return tuple(a[k] + (b[k] - a[k]) * t for k in range(4))


def render(width, height, shade, samples=4):
    """Pixels of a shading function of a point (in pixels, y down), averaged over samples by samples in each."""
    out = bytearray(width * height * 4)
    for y in range(height):
        for x in range(width):
            acc = [0.0, 0.0, 0.0, 0.0]
            for j in range(samples):
                for i in range(samples):
                    c = shade(x + (i + 0.5) / samples, y + (j + 0.5) / samples)
                    for k in range(3):
                        acc[k] += c[k] * c[3]
                    acc[3] += c[3]
            n = samples * samples
            a = acc[3] / n
            rgb = [acc[k] / acc[3] if acc[3] > 0 else 0.0 for k in range(3)]
            o = (y * width + x) * 4
            out[o:o + 4] = bytes(int(round(max(0.0, min(1.0, v)) * 255)) for v in rgb + [a])
    return out


def rounded_box(x, y, x0, y0, x1, y1, r):
    """Whether a point is inside a box with round corners."""
    cx = min(max(x, x0 + r), x1 - r)
    cy = min(max(y, y0 + r), y1 - r)
    return x0 <= x <= x1 and y0 <= y <= y1 and (x - cx) ** 2 + (y - cy) ** 2 <= r * r


def landscape(x, y):
    """A sky over two ranges of mountains, a sun and a meadow (192 by 112)."""
    c = mix(hex_color("#2b4c7e"), hex_color("#f2a65a"), y / 80)
    if (x - 142) ** 2 + (y - 48) ** 2 <= 15 ** 2:
        c = hex_color("#ffe08a")
    if y > 86 - 34 * max(0.0, 1 - abs(x - 60) / 70):
        c = hex_color("#3b4a6b")
    if y > 90 - 26 * max(0.0, 1 - abs(x - 150) / 60):
        c = hex_color("#2d3a55")
    if y > 92:
        c = mix(hex_color("#3f8f5a"), hex_color("#2c6b42"), (y - 92) / 20)
    return c


def avatar(x, y):
    """A face on a teal ground (96 by 96)."""
    d = math.hypot(x - 48, y - 30) / 70
    c = mix(hex_color("#3fd0a8"), hex_color("#1f6f8b"), d)
    if ((x - 48) / 30) ** 2 + ((y - 100) / 26) ** 2 <= 1:
        c = hex_color("#ff8a4c")
    if (x - 48) ** 2 + (y - 42) ** 2 <= 19 ** 2:
        c = hex_color("#ffd2a8")
    for ex in (41, 55):
        if (x - ex) ** 2 + (y - 40) ** 2 <= 2.4 ** 2:
            c = hex_color("#2a303c")
    if 46 <= y <= 49 and abs(x - 48) <= 6 and (x - 48) ** 2 / 36 + (y - 46) ** 2 / 9 <= 1:
        c = hex_color("#c0504d")
    return c


def frame(x, y):
    """A panel for 9-slicing: round corners of 12, a light border of 3 (48 by 48, slices of 16)."""
    if not rounded_box(x, y, 0, 0, 48, 48, 12):
        return (0.0, 0.0, 0.0, 0.0)
    if not rounded_box(x, y, 3, 3, 45, 45, 9):
        return hex_color("#8fc1ff")
    return mix(hex_color("#34405a"), hex_color("#252b38"), y / 48)


def star(x, y):
    """A white five pointed star with alpha, to be tinted (32 by 32)."""
    # Ten corners, the points 15 out and the notches 6 out, the first point up; even odd crossings.
    corners = []
    for i in range(10):
        radius = 15 if i % 2 == 0 else 6
        angle = -math.pi / 2 + i * math.pi / 5
        corners.append((16 + radius * math.cos(angle), 17 + radius * math.sin(angle)))
    inside = False
    for i in range(10):
        (ax, ay), (bx, by) = corners[i], corners[(i + 1) % 10]
        if (ay > y) != (by > y) and x < ax + (y - ay) * (bx - ax) / (by - ay):
            inside = not inside
    return (1.0, 1.0, 1.0, 1.0 if inside else 0.0)


def checker(x, y):
    """Two colors in squares of 5, for round repetition (20 by 20)."""
    return hex_color("#4f9dff") if (int(x // 5) + int(y // 5)) % 2 == 0 else hex_color("#2a303c")


def cursor(x, y):
    """An arrow pointer, its tip at (1, 1): white with a dark border (24 by 24)."""
    def inside(px, py, grow):
        # The arrow's outline: a triangle with a tail.
        tri = py >= 1 - grow and px >= 1 - grow and px <= 1 + (py - 1) * 0.72 + grow and py <= 17 + grow
        tail = 8 - grow <= py <= 22 + grow and abs(px - (5 + (py - 8) * 0.45)) <= 2.2 + grow
        return tri or tail
    if inside(x, y, 0):
        return (1.0, 1.0, 1.0, 1.0)
    if inside(x, y, 1.3):
        return hex_color("#11151c")
    return (0.0, 0.0, 0.0, 0.0)


# The samples' images: path, size, shading.
IMAGES = {"UIRender": [("Images/Landscape.png", 192, 112, landscape), ("Images/Avatar.png", 96, 96, avatar),
                       ("Images/Frame.png", 48, 48, frame), ("Images/Star.png", 32, 32, star)],
          "UIStyles": [("Images/Landscape.png", 192, 112, landscape), ("Images/Star.png", 32, 32, star),
                       ("Images/Checker.png", 20, 20, checker), ("Images/Cursor.png", 24, 24, cursor)]}

# The C# components of a sample's UI object: their script asset and serialized fields (assets of the folder).
SAMPLE_COMPONENTS = {"UIElements": [("Scripts/UIElementsDemo.asset", [("rowTemplate", "UI/Row.hxml"), ("darkTheme", "UI/Dark.hss"),
                                                                     ("lightTheme", "UI/Light.hss")])],
                     "UIControls": [("Scripts/UIControlsDemo.asset", [("worldTemplate", "UI/World.hxml")]), ("Scripts/UIControlsCpp.asset", [])],
                     "UICollections": [("Scripts/UICollectionsDemo.asset", [])],
                     "UIBinding": [("Scripts/UIBindingDemo.asset", [])]}

# Images in a UI: readable (into the UI's atlas), their bytes as they are, no mipmaps, clamped.
IMAGE_META = ("TextureImporter:\n  mipmaps:\n    enableMipMap: 0\n  sRGBTexture: 0\n  isReadable: 1\n"
              "  textureSettings:\n    filterMode: 1\n    wrapU: 1\n    wrapV: 1\n")


def write_bytes(folder, path, data):
    full = os.path.join(folder, path)
    os.makedirs(os.path.dirname(full), exist_ok=True)
    with open(full, "wb") as f:
        f.write(data)


def color(c):
    return f"{{r: {c[0]}, g: {c[1]}, b: {c[2]}, a: {c[3]}}}"


def ui_scene(sample, markup, background):
    """A camera clearing to the background color and a UIView of the markup that watches its files (and the
    sample's C# components beside it)."""
    components = SAMPLE_COMPONENTS.get(sample, [])
    out = [HEADER]
    out.append("--- !h!1 &100\nGameObject:\n  m_Component:\n  - component: {fileID: 101}\n  - component: {fileID: 102}\n"
               "  m_Name: Main Camera\n  m_TagString: MainCamera\n  m_IsActive: 1\n")
    out.append("--- !h!4 &101\nTransform:\n  m_GameObject: {fileID: 100}\n  m_Children: []\n  m_Father: {fileID: 0}\n")
    out.append(f"--- !h!20 &102\nCamera:\n  m_GameObject: {{fileID: 100}}\n  m_Enabled: 1\n  m_ClearFlags: 2\n"
               f"  m_BackGroundColor: {color(background)}\n")
    listed = "".join(f"  - component: {{fileID: {113 + i}}}\n" for i in range(len(components)))
    out.append("--- !h!1 &110\nGameObject:\n  m_Component:\n  - component: {fileID: 111}\n  - component: {fileID: 112}\n" + listed +
               "  m_Name: UI\n  m_IsActive: 1\n")
    out.append("--- !h!4 &111\nTransform:\n  m_GameObject: {fileID: 110}\n  m_Children: []\n  m_Father: {fileID: 0}\n")
    out.append(f"--- !h!114 &112\nGameBehaviour:\n  m_GameObject: {{fileID: 110}}\n  m_Enabled: 1\n"
               f"  m_Script: {{fileID: 11500000, guid: {guid(sample, 'Scripts/UIView.asset')}, type: 3}}\n"
               f"  m_Source: {{fileID: {MAIN_SCRIPT_OBJECT}, guid: {guid(sample, markup)}, type: 3}}\n"
               f"  m_SortingOrder: 0\n  m_WatchFiles: 1\n")
    for i, (script, fields) in enumerate(components):
        values = "".join(f"  {name}: {{fileID: {MAIN_SCRIPT_OBJECT}, guid: {guid(sample, path)}, type: 3}}\n" for name, path in fields)
        out.append(f"--- !h!114 &{113 + i}\nGameBehaviour:\n  m_GameObject: {{fileID: 110}}\n  m_Enabled: 1\n"
                   f"  m_Script: {{fileID: 11500000, guid: {guid(sample, script)}, type: 3}}\n" + values)
    return "".join(out)


# The samples: name, product name, the scene's markup and background color.
SAMPLE_LIST = [("UILayout", "Helix Samples - UI Layout", "UI/UILayout.hxml", (0.08, 0.09, 0.11, 1)),
               ("UIText", "Helix Samples - UI Text", "UI/UIText.hxml", (0.08, 0.09, 0.11, 1)),
               ("UIRender", "Helix Samples - UI Rendering", "UI/UIRender.hxml", (0.08, 0.09, 0.11, 1)),
               ("UIInput", "Helix Samples - UI Input", "UI/UIInput.hxml", (0.08, 0.09, 0.11, 1)),
               ("UIStyles", "Helix Samples - UI Styles", "UI/UIStyles.hxml", (0.08, 0.09, 0.11, 1)),
               ("UIElements", "Helix Samples - UI Element Types", "UI/UIElements.hxml", (0.08, 0.09, 0.11, 1)),
               ("UIControls", "Helix Samples - UI Controls", "UI/UIControls.hxml", (0.11, 0.13, 0.17, 1)),
               ("UICollections", "Helix Samples - UI Collections", "UI/UICollections.hxml", (0.11, 0.13, 0.17, 1)),
               ("UIBinding", "Helix Samples - UI Data Binding", "UI/UIBinding.hxml", (0.11, 0.13, 0.17, 1)),
               ("UINodeGraph", "Helix Samples - UI Node Graph", "UI/UINodeGraph.hxml", (0.13, 0.13, 0.13, 1))]


def main():
    for sample, product, markup, background in SAMPLE_LIST:
        data = os.path.join(SAMPLES, sample, "Data")
        for asset, module, namespace, name in SCRIPTS + SAMPLE_SCRIPTS.get(sample, []):
            write(data, f"Scripts/{asset}.asset", script_asset(module, namespace, name))
        write(data, f"Scenes/{sample}.helix", ui_scene(sample, markup, background))
        for path, width, height, shade in IMAGES.get(sample, []):
            write_bytes(data, path, png(width, height, render(width, height, shade)))
        # Every image of the sample (its own and the CC0 art of Tools/PrepareImages.py) gets its .meta.
        for folder, _, names in os.walk(data):
            for n in names:
                if n.lower().endswith(".png"):
                    write(folder, n + ".meta", IMAGE_META)
        write(data, "settings.asset",
              f"{HEADER}--- !h!129 &1\nPlayerSettings:\n  companyName: Helix\n  productName: {product}\n  bundleVersion: 1.0\n"
              f"  applicationIdentifier: org.helix.samples.{sample.lower()}\n  runInBackground: 1\n"
              f"--- !h!141 &2\nBuildSettings:\n  scenes:\n  - Scenes/{sample}.helix\n")
        files = []
        for folder, _, names in os.walk(data):
            for n in names:
                path = os.path.relpath(os.path.join(folder, n), data).replace(os.sep, "/")
                if path not in ("assets.index", "settings.asset") and not path.endswith(".meta"):
                    files.append(path)
        lines = ["# The sample's data folder: every file, its guid and its path (Tools/GenerateData.py)."]
        lines += [f"{guid(sample, p)} {p}" for p in sorted(files)]
        write(data, "assets.index", "\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
