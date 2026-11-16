#include <Helix/Samples/Launcher/Launcher.h>

#include <Helix/Samples/Charts/Chart.h>

#include <Helix/Core/Logging/Log.h>
#include <Helix/Core/Strings/Format.h>
#include <Helix/Engine/Asset/Asset.h>
#include <Helix/Engine/Rendering/Pipeline/RenderPipeline.h>
#include <Helix/Engine/Script/ScriptRuntime.h>
#include <Helix/Engine/Script/ScriptableObject.h>
#include <Helix/Platform/IO/FileSystem.h>
#include <Helix/Platform/Window/Window.h>
#include <Helix/Player/Host/Player.h>
#include <Helix/RenderPipelines/Physical/Pipeline/PhysicalRenderPipelineAsset.h>
#include <Helix/RenderPipelines/Standard/Pipeline/UniversalRenderPipelineAsset.h>

extern "C" void HxBindings_HelixRenderPipelinesCore_Publish();
extern "C" void HxBindings_HelixRenderPipelinesStandard_Publish();
extern "C" void HxBindings_HelixRenderPipelinesPhysical_Publish();
extern "C" void HxBindings_HelixUI_Publish();

namespace helix::samples
{
    namespace
    {
        LogChannel SamplesLog{"Samples"};

        engine::ScriptableObject* CreateStandard() { return rendering::universal::UniversalRenderPipelineAsset::Create(); }
        engine::ScriptableObject* CreatePhysical() { return rendering::physical::PhysicalRenderPipelineAsset::Create(); }

        constexpr PipelineEntry kPipelines[] = {
            {"standard", "HelixRenderPipelinesStandard", &HxBindings_HelixRenderPipelinesStandard_Publish, &CreateStandard},
            {"physical", "HelixRenderPipelinesPhysical", &HxBindings_HelixRenderPipelinesPhysical_Publish, &CreatePhysical},
            {"none", "", nullptr, nullptr},
        };

        // The modules every sample has beside the pipelines.
        struct ModuleEntry
        {
            StringView name;
            void (*publish)();
        };
        constexpr ModuleEntry kModules[] = {
            {"HelixRenderPipelinesCore", &HxBindings_HelixRenderPipelinesCore_Publish},
            {"HelixUI", &HxBindings_HelixUI_Publish},
        };

        bool Register(StringView name, void (*publish)())
        {
            publish();
            String error;
            if (engine::RegisterLinkedGameModule(name, error))
                return true;
            HX_LOG_ERROR(SamplesLog, "The module {} did not register: {}", name, error);
            return false;
        }

        // The shader assemblies beside the executable, loaded as assets (the pipelines find their shaders by name).
        engine::FolderAssetSource* LoadShaders()
        {
            String folder = platform::JoinPath(platform::GetDirectoryName(platform::ExecutablePath()), "Shaders");
            if (!platform::DirectoryExists(folder))
            {
                HX_LOG_ERROR(SamplesLog, "No shader folder at '{}'.", folder);
                return nullptr;
            }
            auto* source = new engine::FolderAssetSource(folder);
            engine::AddAssetSource(source);
            Array<engine::AssetEntry> entries;
            source->List(entries);
            u32 loaded = 0;
            for (const engine::AssetEntry& e : entries)
                if (platform::GetExtension(e.path.View()) == ".dll" && engine::LoadAsset(e.guid))
                    loaded++;
            HX_LOG_INFO(SamplesLog, "{} shader assemblies loaded from '{}'.", loaded, folder);
            return source;
        }

        struct Run
        {
            player::Player player;
        };

        bool RunFrame(void* user) { return static_cast<Run*>(user)->player.Frame(); }
    }

    Span<const PipelineEntry> Pipelines() { return Span<const PipelineEntry>(kPipelines, sizeof(kPipelines) / sizeof(kPipelines[0])); }

    const PipelineEntry* FindPipeline(StringView name)
    {
        for (const PipelineEntry& p : Pipelines())
            if (p.name == name)
                return &p;
        return nullptr;
    }

    bool ParseLauncherArgs(Span<const StringView> args, LauncherOptions& out, Array<String>& errors)
    {
        usize before = errors.Size();
        out.pipeline = String(Pipelines()[0].name);
        for (usize i = 0; i < args.Size(); i++)
        {
            StringView a = args[i];
            if (a == "-pipeline" || a == "-sample")
            {
                if (i + 1 >= args.Size())
                {
                    errors.Push(Format("{} needs a value", a));
                    continue;
                }
                StringView v = args[++i];
                if (a == "-sample")
                    out.sample = String(v);
                else if (FindPipeline(v))
                    out.pipeline = String(v);
                else
                {
                    String known;
                    for (const PipelineEntry& p : Pipelines())
                        known += Format("{}{}", known.Empty() ? "" : ", ", p.name);
                    errors.Push(Format("-pipeline takes one of {}, not '{}'", known.View(), v));
                }
                continue;
            }
            out.playerArgs.Push(String(a));
        }
        return errors.Size() == before;
    }

    i32 RunLauncher(Span<const StringView> args)
    {
        LauncherOptions launcher;
        Array<String> errors;
        if (!ParseLauncherArgs(args, launcher, errors))
            for (const String& e : errors)
                HX_LOG_ERROR(SamplesLog, "Command line: {}", e);

        // The player's arguments, with the sample's data folder unless one is given.
        Array<StringView> playerArgs;
        bool dataFolder = false;
        for (const String& a : launcher.playerArgs)
        {
            playerArgs.Push(a.View());
            dataFolder = dataFolder || a == "-dataFolder";
        }
        String sampleData = platform::JoinPath(platform::JoinPath(HELIX_SAMPLES_DIR, launcher.sample.View()), "Data");
        if (!dataFolder)
        {
            playerArgs.Push("-dataFolder");
            playerArgs.Push(sampleData.View());
        }
        player::PlayerOptions options;
        errors.Clear();
        if (!player::ParseCommandLine(Span<const StringView>(playerArgs.Data(), playerArgs.Size()), options, errors))
            for (const String& e : errors)
                HX_LOG_ERROR(SamplesLog, "Command line: {}", e);

        const PipelineEntry* pipeline = FindPipeline(launcher.pipeline.View());
        bool registered = true;
        for (const ModuleEntry& m : kModules)
            registered = Register(m.name, m.publish) && registered;
        for (const PipelineEntry& p : Pipelines())
            if (p.publish)
                registered = Register(p.module, p.publish) && registered;
        if (!registered)
            return 1;
        RegisterSampleChart();

        Run run;
        if (!run.player.Start(options))
        {
            i32 code = run.player.Running() ? 1 : 0;
            run.player.Shutdown();
            return code;
        }
        engine::FolderAssetSource* shaders = LoadShaders();
        engine::ScriptableObject* asset = pipeline->createAsset ? pipeline->createAsset() : nullptr;
        bool made = asset || !pipeline->createAsset;
        if (asset)
        {
            engine::GraphicsSettings::SetDefaultRenderPipeline(asset);
            HX_LOG_INFO(SamplesLog, "Sample {} on the {} pipeline.", launcher.sample.View(), pipeline->name);
        }
        else if (made)
            HX_LOG_INFO(SamplesLog, "Sample {} without a render pipeline.", launcher.sample.View());
        else
            HX_LOG_ERROR(SamplesLog, "The {} pipeline's asset could not be made.", pipeline->name);
        platform::RunMainLoop(&RunFrame, &run);
        i32 code = run.player.ExitCode();
        engine::GraphicsSettings::SetDefaultRenderPipeline(nullptr);
        run.player.Shutdown();
        delete shaders;
        return made && shaders ? code : 1;
    }
}
