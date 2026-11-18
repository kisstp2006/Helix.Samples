"""Builds the AssetDemo project's editor scripts against an editor build and opens the project in that editor:
python Tools/OpenAssetDemo.py <path to HelixEditor(.exe)> [editor arguments...]
The scripts go to Projects/AssetDemo/Library/ScriptAssemblies (the editor loads a project's script assemblies from
there); the editor's Managed folder beside its executable is what they are built against.
"""

import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROJECT = os.path.join(ROOT, "Projects", "AssetDemo")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    editor = os.path.abspath(sys.argv[1])
    managed = os.path.join(os.path.dirname(editor), "Managed")
    if not os.path.isfile(os.path.join(managed, "HelixEditor.CoreModule.dll")):
        print(f"No editor assemblies in {managed}.")
        return 1
    csproj = os.path.join(PROJECT, "Scripts", "AssetDemo.Scripts.csproj")
    build = subprocess.run(["dotnet", "build", csproj, "-c", "Release", "-nologo", "-v", "q", f"-p:HelixEditorManaged={managed}"])
    if build.returncode != 0:
        return build.returncode
    return subprocess.run([editor, "-projectPath", PROJECT] + sys.argv[2:]).returncode


if __name__ == "__main__":
    sys.exit(main())
