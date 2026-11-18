"""Builds an editor project's scripts against an editor build and opens the project in that editor:
python Tools/OpenProject.py <project: AssetDemo, InspectorDemo, SceneDemo> <path to HelixEditor(.exe)> [editor arguments...]
The scripts (Projects/<project>/Scripts/*.csproj) go to Projects/<project>/Library/ScriptAssemblies (the editor loads
a project's script assemblies from there); the editor's Managed folder beside its executable is what they are built
against.
"""

import glob
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    project = os.path.join(ROOT, "Projects", sys.argv[1])
    if not os.path.isdir(project):
        print(f"No project {sys.argv[1]} in {os.path.join(ROOT, 'Projects')}.")
        return 1
    editor = os.path.abspath(sys.argv[2])
    managed = os.path.join(os.path.dirname(editor), "Managed")
    if not os.path.isfile(os.path.join(managed, "HelixEditor.CoreModule.dll")):
        print(f"No editor assemblies in {managed}.")
        return 1
    for csproj in sorted(glob.glob(os.path.join(project, "Scripts", "*.csproj"))):
        build = subprocess.run(["dotnet", "build", csproj, "-c", "Release", "-nologo", "-v", "q", f"-p:HelixEditorManaged={managed}"])
        if build.returncode != 0:
            return build.returncode
    return subprocess.run([editor, "-projectPath", project] + sys.argv[3:]).returncode


if __name__ == "__main__":
    sys.exit(main())
