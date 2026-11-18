# Helix.Samples

Samples of the Helix engine, run by a launcher: the player with both render pipelines, the UI and the canvas UI built in.

## Samples

| Sample | Shows |
|---|---|
| `UILayout` | A page of HXML markup and HSS styles: rows and columns, wrapping, alignment, aspect ratios, percentages, borders, opacity, variables and templates. Edit `Samples/UILayout/Data/UI/UILayout.hss` or `UILayout.hxml` while it runs: the page changes within a second. |
| `UIText` | Text in labels: rich text tags, Hebrew and Arabic within English and an Arabic paragraph, Chinese, Japanese and Korean with their line rules, color emoji, an ellipsis and wrapping, drawn with the machine's fonts. Edit `Samples/UIText/Data/UI/UIText.hss` or `UIText.hxml` while it runs. |
| `UIInput` | Input: buttons with hover, pressed and focus styles, the focus ring by Tab and Shift+Tab, text boxes that take typed text and show what an input method composes, cards dragged between two lists, and a log of the events in their order, all made by the launcher's sample element types on the panel's events. |
| `UIStyles` | Styles: tiles turned, scaled and moved (back with a transition on hover), a turned box that clips its children, backgrounds repeated, spaced, rounded, placed and contained, text with a shadow and an outline, the ellipsis at the end, the start and the middle, cursors (a link shape, an image), and a theme (`.tss`) that imports the sheet of the look. Its images are its own beside CC0 art of Kenney and Poly Haven (`Samples/UIStyles/CREDITS.txt`). |
| `UIElements` | Element types of scripts: `StatCard`, a C# class (`Samples/UIElements/Scripts`) with attributes from markup and its own content container, holding `Gauge`, a C++ class of the launcher's game module; a C# script clones rows from a template into a list and switches the theme sheet on the root. |
| `UIControls` | The controls in Helix's default theme: buttons, a repeat button, toggles, radio buttons and groups, a toggle button group, sliders, a progress bar, help boxes; text, password, number, vector and rect fields, a dropdown, a mask field, a hash field; a foldout, a scroll view, a split view, a popup window in tabs; controls made from C# (`Samples/UIControls/Scripts`) and from a C++ script of the launcher's module (`Source/Helix/Samples/Scripts/UIControlsCpp.h`: a slider, a toggle, a number and a button through the C++ script API, heard by their events) that show each change in the status line; a panel of world space settings in the scene, clicked and typed into through the main camera, with a part turned in 3D. |
| `UICollections` | Collections, menus and painting from C# (`Samples/UICollections/Scripts`): a list of 500 items (only the rows in view are made) with a foldout header, an add/remove footer, multiple selection and rows moved by dragging, a contextual menu on its rows; a tree of a scene's objects; a table of assets in three columns that sorts, resizes and hides its columns from the header; a dropdown menu, tooltips; a chart drawn by `generateVisualContent`: bars of a vertical gradient (a mesh of `Allocate`), a smooth curve over a faint area, axes and labels, a vector image, the hovered bar's value; a donut of each stage's share with its legend; the bars grow in with an animation and a meter is animated by its style values. |
| `UIBinding` | Data binding: a character sheet (`Samples/UIBinding/Scripts`) as the page's data source; fields bound both ways in markup (`<Bindings>`, a section of its own `data-source-path`), a label through a converter group, a progress bar and a summary bound from C#, a binding class of the sample's; buttons change the object and the fields follow. |
| `UINodeGraph` | A node graph editor in the reference editor's dark skin, its logic in the launcher's C++ (`Source/Helix/Samples/Graph`): port types and node kinds in registries (input, math, procedural, UV, channel; the Vertex and Fragment contexts with their blocks), evaluated per pixel on the CPU for every node's preview and a lit sphere in the main preview; nodes with category stripes, ports colored by type, inline values of unconnected inputs, bezier edges. Drag a node to move it, drag a port to connect it (from a connected input: to take the edge off), drag an inline value to change it, wheel to zoom, middle button or Alt to pan, a band to select, Delete to remove, F to frame, a right click or Space for the create node menu. The blackboard, the graph inspector (following the selection) and the main preview float over the graph. |
| `UIImmediate` | The immediate GUI: the C# script's `OnGui` (`Samples/UIImmediate/Scripts`) draws a HUD, a laid out settings window in pages (fields, a password field, toggles, sliders, toolbars, a selection grid), a debug console window (the log as it comes, a line to log; the wrench of the dock opens it), a window with a scrolled list of companions with their pictures and repeat buttons, a notes window with a text area and the clipboard, a dock of icon buttons with tooltips and a medal turned by the GUI's matrix; the launcher's C++ script (`Source/Helix/Samples/Scripts/UIImmediateCpp.h`) lays out an area of its own through the C++ script API; all over a UI panel of markup. Drag a window by its title; a click brings it to the front; Tab moves through the text fields. Its images are its own beside CC0 art of Kenney (`Samples/UIImmediate/CREDITS.txt`). |
| `UICanvas` | The canvas UI (GameObjects): a HUD over a 3D yard (a player card with a filled health bar, a star), an inventory of icons in a scrolled grid with a vertical scrollbar, settings (a toggle that hides the yard's labels, a slider for the camera's field of view, a dropdown, a name field), a world space terminal on a post in the yard clicked and typed into through the camera (`open`, `heal`), world text labels over the crates, a status line; a panel of a button, a toggle and a slider made by the launcher's C++ script (`Source/Helix/Samples/Scripts/UICanvasCpp.h`) through the canvas UI's C++ script API. The C# script is `Samples/UICanvas/Scripts/UICanvasDemo.cs`. Its images are its own beside CC0 art of Kenney (`Samples/UICanvas/CREDITS.txt`). |
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
HelixSamples -sample UICollections
HelixSamples -sample UIBinding
HelixSamples -sample UINodeGraph
HelixSamples -sample UIImmediate
HelixSamples -sample UICanvas -pipeline physical
```

`-pipeline` picks the render pipeline (`standard` by default; `none` runs without one: the engine clears the cameras and
the UI draws over them), `-sample` the sample (`UILayout` by default). Every argument of the player works too:
`-force-glcore` for OpenGL, `-force-device-index <n>` for another graphics adapter, `-screen-width` and `-screen-height`
for the window's size. The launcher reads the samples' data folders where they are in the repository, so edits show at
once. A sample's C# scripts (`Samples/<name>/Scripts`) are built beside the launcher into `Managed/<name>` with the
engine's assemblies, the folder the launcher gives the player.

## Editor projects

`Projects/AssetDemo` is a project of the Helix editor (Helix.Editor): its own textures, a pixel art sprite, models and
palettes (drawn by `Tools/GenerateAssetDemo.py`) and editor scripts (`Projects/AssetDemo/Scripts/AssetDemo.cs`):
`PaletteImporter`, a scripted importer of `.palette` files (a color a line) into strip textures; `PixelArtPostprocessor`,
an asset postprocessor that imports the sprites under `Assets/Textures/Pixel/` with point filtering; and the window
Window > Asset Demo, a tab beside the scene view listing the assets with their main types, importers, previews and
import hashes. The editor imports the project into its `Library/` (the artifacts the next open loads from).

```
python Tools/OpenProject.py AssetDemo <path to HelixEditor.exe>
python Tools/OpenProject.py AssetDemo <path to HelixEditor.exe> -pipeline physical -force-glcore
```

builds the scripts against that editor's assemblies into `Projects/AssetDemo/Library/ScriptAssemblies` (where the
editor loads a project's script assemblies from) and opens the project; the arguments after the editor's path go to it.

`Projects/InspectorDemo` is the editor's Inspector at work: a scene (`Assets/Scenes/InspectorDemo.helix`) of two
spinners, a lamp with a light and a marker, and the default camera and light; its scripts
(`Projects/InspectorDemo/Scripts/InspectorDemo.cs`) are the components `Spinner` (Range, Tooltip, Header, Space,
Multiline, Min, an enum, a color, an array, a reference and a struct with a property drawer) and `Marker` (the default
inspector), the custom editor `SpinnerEditor` (several spinners at once, a help box and a button) and the drawer
`LimitsDrawer`; both components are under Demo in Add Component. Demo > Open Inspector Demo opens the scene with a
spinner selected (made and saved there the first time):

```
python Tools/OpenProject.py InspectorDemo <path to HelixEditor.exe> -executeMethod InspectorDemo.DemoScene.Show
```

## Contents

| Path | Content |
|---|---|
| `Source/Helix/Samples/Launcher/` | The launcher: the pipelines it knows (name, module, how its asset is made), the modules it registers, the shader folder it loads |
| `Source/Helix/Samples/Elements/` | `Gauge`: the C++ element type of `UIElements`, in the launcher's game module HelixSamples (Helix.Bindgen registers it with the UI) |
| `Source/Helix/Samples/Interaction/` | The sample element types of `UIInput` (a button, a text box, drag items and drop lists, a probe and a log of events) |
| `Source/Helix/Samples/Charts/` | The `SampleChart` element type: charts of the values in its markup, painted by kind from a table |
| `Samples/<name>/Data/` | Each sample's data folder: its own files, and the settings, scene, script assets, images, their .meta and the index a build would leave (written by `Tools/GenerateData.py`) |
| `Projects/AssetDemo/` | The editor project above: its assets with their .meta files, its editor scripts and their project file |
| `Projects/InspectorDemo/` | The editor project of the Inspector: its scene, its scripts' assets with their .meta files, its scripts and their project file |
| `Tools/GenerateAssetDemo.py`, `Tools/OpenProject.py` | Draw the AssetDemo project's assets; build an editor project's scripts and open it in an editor |
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
