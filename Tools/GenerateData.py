"""Writes what a build would leave in each sample's data folder (Samples/<name>/Data) beside the sample's own files:
settings.asset, the script assets (MonoScript) the scene names, the scene and assets.index, which lists every file of
the folder. The sample's own files (markup, style sheets) are left as they are. Run it after adding a file to a
sample: python Tools/GenerateData.py
"""

import hashlib
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SAMPLES = os.path.join(ROOT, "Samples")
HEADER = "%YAML 1.1\n%TAG !h! tag:helixengine,2026:\n"

# The C++ script classes the samples' scenes use: (asset name, module, namespace, class).
SCRIPTS = [("UIView", "HelixUI", "HelixEngine.UI", "UIView")]

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


def color(c):
    return f"{{r: {c[0]}, g: {c[1]}, b: {c[2]}, a: {c[3]}}}"


def ui_scene(sample, markup, background):
    """A camera clearing to the background color and a UIView of the markup that watches its files."""
    out = [HEADER]
    out.append("--- !h!1 &100\nGameObject:\n  m_Component:\n  - component: {fileID: 101}\n  - component: {fileID: 102}\n"
               "  m_Name: Main Camera\n  m_IsActive: 1\n")
    out.append("--- !h!4 &101\nTransform:\n  m_GameObject: {fileID: 100}\n  m_Children: []\n  m_Father: {fileID: 0}\n")
    out.append(f"--- !h!20 &102\nCamera:\n  m_GameObject: {{fileID: 100}}\n  m_Enabled: 1\n  m_ClearFlags: 2\n"
               f"  m_BackGroundColor: {color(background)}\n")
    out.append("--- !h!1 &110\nGameObject:\n  m_Component:\n  - component: {fileID: 111}\n  - component: {fileID: 112}\n"
               "  m_Name: UI\n  m_IsActive: 1\n")
    out.append("--- !h!4 &111\nTransform:\n  m_GameObject: {fileID: 110}\n  m_Children: []\n  m_Father: {fileID: 0}\n")
    out.append(f"--- !h!114 &112\nGameBehaviour:\n  m_GameObject: {{fileID: 110}}\n  m_Enabled: 1\n"
               f"  m_Script: {{fileID: 11500000, guid: {guid(sample, 'Scripts/UIView.asset')}, type: 3}}\n"
               f"  m_Source: {{fileID: {MAIN_SCRIPT_OBJECT}, guid: {guid(sample, markup)}, type: 3}}\n"
               f"  m_SortingOrder: 0\n  m_WatchFiles: 1\n")
    return "".join(out)


# The samples: name, product name, the scene's markup and background color.
SAMPLE_LIST = [("UILayout", "Helix Samples - UI Layout", "UI/UILayout.hxml", (0.08, 0.09, 0.11, 1)),
               ("UIText", "Helix Samples - UI Text", "UI/UIText.hxml", (0.08, 0.09, 0.11, 1))]


def main():
    for sample, product, markup, background in SAMPLE_LIST:
        data = os.path.join(SAMPLES, sample, "Data")
        for asset, module, namespace, name in SCRIPTS:
            write(data, f"Scripts/{asset}.asset", script_asset(module, namespace, name))
        write(data, f"Scenes/{sample}.helix", ui_scene(sample, markup, background))
        write(data, "settings.asset",
              f"{HEADER}--- !h!129 &1\nPlayerSettings:\n  companyName: Helix\n  productName: {product}\n  bundleVersion: 1.0\n"
              f"  applicationIdentifier: org.helix.samples.{sample.lower()}\n  runInBackground: 1\n"
              f"--- !h!141 &2\nBuildSettings:\n  scenes:\n  - Scenes/{sample}.helix\n")
        files = []
        for folder, _, names in os.walk(data):
            for n in names:
                path = os.path.relpath(os.path.join(folder, n), data).replace(os.sep, "/")
                if path not in ("assets.index", "settings.asset"):
                    files.append(path)
        lines = ["# The sample's data folder: every file, its guid and its path (Tools/GenerateData.py)."]
        lines += [f"{guid(sample, p)} {p}" for p in sorted(files)]
        write(data, "assets.index", "\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
