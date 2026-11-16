# Helix.Samples

Samples of the Helix engine, run by a launcher: the player with both render pipelines and the UI built in.

## Samples

| Sample | Shows |
|---|---|
| `UILayout` | A page of HXML markup and HSS styles: rows and columns, wrapping, alignment, aspect ratios, percentages, borders, opacity, variables and templates. Edit `Samples/UILayout/Data/UI/UILayout.hss` or `UILayout.hxml` while it runs: the page changes within a second. |
| `UIText` | Text in labels: rich text tags, Hebrew and Arabic within English and an Arabic paragraph, Chinese, Japanese and Korean with their line rules, color emoji, an ellipsis and wrapping, drawn with the machine's fonts. Edit `Samples/UIText/Data/UI/UIText.hss` or `UIText.hxml` while it runs. |

## Running

```
HelixSamples -sample UILayout -pipeline standard
HelixSamples -sample UILayout -pipeline physical
HelixSamples -sample UIText -pipeline standard
```

`-pipeline` picks the render pipeline (`standard` by default), `-sample` the sample (`UILayout` by default). Every
argument of the player works too: `-force-glcore` for OpenGL, `-force-device-index <n>` for another graphics adapter,
`-screen-width` and `-screen-height` for the window's size. The launcher reads the samples' data folders where they
are in the repository, so edits show at once.

## Contents

| Path | Content |
|---|---|
| `Source/Helix/Samples/Launcher/` | The launcher: the pipelines it knows (name, module, asset class), the modules it registers, the shader folder it loads |
| `Samples/<name>/Data/` | Each sample's data folder: its own files, and the settings, scene, script assets and index a build would leave (written by `Tools/GenerateData.py`) |
| `Tests/` | Each sample on each pipeline in batch mode, its screenshot against `Tests/Golden/` (samples drawn with the machine's fonts are compared only off CI) |
| `Tools/CompareImages/` | The screenshot comparison of the tests |

## Build

```
git clone https://github.com/kisstp2006/Helix.Samples
git -C Helix.Samples submodule update --init
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
```

The .NET 11 SDK is needed (`HELIX_DOTNET_ROOT` or `DOTNET_ROOT`). The launcher is built on Windows and Linux.

## License

GPL-3.0, see `LICENSE`.
