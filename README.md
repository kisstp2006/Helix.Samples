# Helix.Samples

Samples of the Helix engine, run by a launcher: the player with both render pipelines and the UI built in.

## Samples

| Sample | Shows |
|---|---|
| `UILayout` | A page of HXML markup and HSS styles: rows and columns, wrapping, alignment, aspect ratios, percentages, borders, opacity, variables and templates. Edit `Samples/UILayout/Data/UI/UILayout.hss` or `UILayout.hxml` while it runs: the page changes within a second. |
| `UIText` | Text in labels: rich text tags, Hebrew and Arabic within English and an Arabic paragraph, Chinese, Japanese and Korean with their line rules, color emoji, an ellipsis and wrapping, drawn with the machine's fonts. Edit `Samples/UIText/Data/UI/UIText.hss` or `UIText.hxml` while it runs. |
| `UIInput` | Input: buttons with hover, pressed and focus styles, the focus ring by Tab and Shift+Tab, text boxes that take typed text and show what an input method composes, cards dragged between two lists, and a log of the events in their order, all made by the launcher's sample element types on the panel's events. |
| `UIStyles` | Styles: tiles turned, scaled and moved (back with a transition on hover), a turned box that clips its children, backgrounds repeated, spaced, rounded, placed and contained, text with a shadow and an outline, the ellipsis at the end, the start and the middle, cursors (a link shape, an image), and a theme (`.tss`) that imports the sheet of the look. Its images are its own beside CC0 art of Kenney and Poly Haven (`Samples/UIStyles/CREDITS.txt`). |
| `UIElements` | Element types of scripts: `StatCard`, a C# class (`Samples/UIElements/Scripts`) with attributes from markup and its own content container, holding `Gauge`, a C++ class of the launcher's game module; a C# script clones rows from a template into a list and switches the theme sheet on the root. |
| `UIControls` | The controls in Helix's default theme: buttons, a repeat button, toggles, radio buttons and groups, a toggle button group, sliders, a progress bar, help boxes; text, password, number, vector and rect fields, a dropdown, a mask field, a hash field; a foldout, a scroll view, a split view, a popup window in tabs; controls made from C# (`Samples/UIControls/Scripts`) that show each change in the status line; a panel of world space settings in the scene, clicked and typed into through the main camera, with a part turned in 3D. |
| `UIRender` | Drawing: rounded boxes, borders of four colors, a circle, images stretched, cropped and fitted, tinted icons, 9-slice buttons and panels, a list and an image clipped to rounded and round boxes, and charts painted with vector paths (a line with a gradient, bars, a donut) by the launcher's `SampleChart` element. Its images are its own (drawn by `Tools/GenerateData.py`) beside CC0 art of Kenney and Poly Haven (`Samples/UIRender/CREDITS.txt`). |

## Running

```
HelixSamples -sample UILayout -pipeline standard
HelixSamples -sample UILayout -pipeline physical
HelixSamples -sample UIText -pipeline standard
HelixSamples -sample UIRender -pipeline none
HelixSamples -sample UIInput
HelixSamples -sample UIStyles
HelixSamples -sample UIElements
HelixSamples -sample UIControls
```

`-pipeline` picks the render pipeline (`standard` by default; `none` runs without one: the engine clears the cameras and
the UI draws over them), `-sample` the sample (`UILayout` by default). Every argument of the player works too:
`-force-glcore` for OpenGL, `-force-device-index <n>` for another graphics adapter, `-screen-width` and `-screen-height`
for the window's size. The launcher reads the samples' data folders where they are in the repository, so edits show at
once. A sample's C# scripts (`Samples/<name>/Scripts`) are built beside the launcher into `Managed/<name>` with the
engine's assemblies, the folder the launcher gives the player.

## Contents

| Path | Content |
|---|---|
| `Source/Helix/Samples/Launcher/` | The launcher: the pipelines it knows (name, module, how its asset is made), the modules it registers, the shader folder it loads |
| `Source/Helix/Samples/Elements/` | `Gauge`: the C++ element type of `UIElements`, in the launcher's game module HelixSamples (Helix.Bindgen registers it with the UI) |
| `Source/Helix/Samples/Interaction/` | The sample element types of `UIInput` (a button, a text box, drag items and drop lists, a probe and a log of events) |
| `Source/Helix/Samples/Charts/` | The `SampleChart` element type: charts of the values in its markup, painted by kind from a table |
| `Samples/<name>/Data/` | Each sample's data folder: its own files, and the settings, scene, script assets, images, their .meta and the index a build would leave (written by `Tools/GenerateData.py`) |
| `Tools/PrepareImages.py` | Puts the CC0 art into the data folders from the downloaded packs (listed in each sample's `CREDITS.txt`) |
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
