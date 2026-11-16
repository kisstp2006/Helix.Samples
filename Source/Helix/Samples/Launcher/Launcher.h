#pragma once

#include <Helix/Core/Basics/Span.h>
#include <Helix/Core/Containers/Array.h>
#include <Helix/Core/Strings/String.h>

// The samples' launcher: the player with the render pipelines and the UI linked in. It takes the player's command line
// and its own arguments:
//   -pipeline <name>  the pipeline that renders (standard, physical; standard by default)
//   -sample <name>    the sample whose data folder runs (Samples/<name>/Data of the repo; UILayout by default)
// The shader assemblies of the pipelines and the UI are an asset folder beside the executable (Shaders/).
namespace helix::samples
{
    // A pipeline the launcher can render with: its name on the command line, its game module and its asset's class.
    struct PipelineEntry
    {
        StringView name;
        StringView module;
        void (*publish)() = nullptr;
        StringView assetClass;
    };

    // The pipelines, the first one the default.
    Span<const PipelineEntry> Pipelines();
    const PipelineEntry* FindPipeline(StringView name);

    // What the launcher takes out of the command line; the rest goes to the player.
    struct LauncherOptions
    {
        String pipeline;
        String sample = String("UILayout");
        Array<String> playerArgs = Array<String>();
    };
    bool ParseLauncherArgs(Span<const StringView> args, LauncherOptions& out, Array<String>& errors);

    // A whole run: the modules registered, the player started on the sample, the pipeline set, the main loop.
    i32 RunLauncher(Span<const StringView> args);
}
