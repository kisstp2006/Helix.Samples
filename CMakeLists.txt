cmake_minimum_required(VERSION 3.28)
project(Helix.Samples LANGUAGES CXX)

if(NOT COMMAND helix_add_library)
    include("${CMAKE_CURRENT_SOURCE_DIR}/External/Helix.CMake/Helix.cmake")
endif()

# The samples: data folders under Samples/<name>/Data run by the launcher HelixSamples, the player with both render
# pipelines and the UI linked in (-pipeline standard|physical|none, -sample <name>). Desktop only: the Web and Android get
# data folders with the build pipeline.
helix_require(Core)
helix_require(Engine)
helix_require(RenderPipelines.Standard)
helix_require(RenderPipelines.Physical)
helix_require(UI)
helix_require(Player)

if(HELIX_PLATFORM STREQUAL "windows" OR HELIX_PLATFORM STREQUAL "linux")
    helix_collect_sources(launcher_sources "${CMAKE_CURRENT_SOURCE_DIR}/Source/Helix")
    file(GLOB_RECURSE launcher_headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Source/Helix/*.h")
    helix_add_executable(helix_samples SOURCES ${launcher_sources} ${launcher_headers}
                         DEPS Helix::PlayerCore Helix::RenderPipelines.Standard Helix::RenderPipelines.Physical Helix::UIEngine)
    set_target_properties(helix_samples PROPERTIES OUTPUT_NAME HelixSamples RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
    target_include_directories(helix_samples PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
    # The samples' data folders are read where they are, so an edit of a sample's files shows while it runs.
    target_compile_definitions(helix_samples PRIVATE "HELIX_SAMPLES_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/Samples\"")

    # The launcher's game module HelixSamples: its C++ element types (Gauge) and scripts (UIControlsCpp), registered like a game's.
    helix_dependency_dir(samples_engine_dir Engine)
    helix_dependency_dir(samples_ui_dir UI)
    helix_dependency_dir(samples_math_dir Math)
    set(samples_engine_source "${samples_engine_dir}/Source")
    set(samples_ui_engine_source "${samples_ui_dir}/Source/Helix.UI.Engine")
    set(samples_references "${samples_engine_source}/Helix/Engine/Object/EntityId.h" "${samples_engine_source}/Helix/Engine/Object/Object.h"
        "${samples_engine_source}/Helix/Engine/GameObject/GameObject.h" "${samples_engine_source}/Helix/Engine/Script/GameBehaviour.h"
        "${samples_engine_source}/Helix/Engine/Script/ScriptableObject.h" "${samples_engine_source}/Helix/Engine/Transform/Transform.h"
        "${samples_engine_source}/Helix/Engine/Texture/Texture.h" "${samples_math_dir}/Source/Helix/Math/Vector.h"
        "${samples_math_dir}/Source/Helix/Math/Color.h" "${samples_math_dir}/Source/Helix/Math/Matrix4x4.h"
        "${samples_math_dir}/Source/Helix/Math/Geometry.h" "${samples_ui_dir}/Source/Helix/UI/Elements/ElementTree.h"
        "${samples_ui_engine_source}/Helix/UI/Engine/Element.h" "${samples_ui_engine_source}/Helix/UI/Engine/UIAssets.h"
        "${samples_ui_engine_source}/Helix/UI/Engine/FontAsset.h" "${samples_ui_engine_source}/Helix/UI/Engine/UIView.h"
        "${samples_ui_engine_source}/Helix/UI/Engine/TemplateContainer.h")
    helix_bindgen(helix_samples MODULE HelixSamples NAMESPACE HelixSamples KIND NativeGame
                  HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/Source/Helix/Samples/Elements/Gauge.h" "${CMAKE_CURRENT_SOURCE_DIR}/Source/Helix/Samples/Scripts/UIControlsCpp.h"
                          "${CMAKE_CURRENT_SOURCE_DIR}/Source/Helix/Samples/Scripts/UIImmediateCpp.h"
                  REFERENCES ${samples_references} REFERENCE_NAMESPACE HelixEngine
                  INCLUDE_ROOTS "${CMAKE_CURRENT_SOURCE_DIR}/Source" "${samples_engine_source}" "${samples_math_dir}/Source"
                                "${samples_ui_engine_source}" "${samples_ui_dir}/Source")

    # The samples' C# scripts (Samples/<name>/Scripts): built beside the launcher into Managed/<name> with the engine's
    # assemblies, the folder the launcher gives the player (the data folders stay sources).
    set(samples_managed_root "${CMAKE_BINARY_DIR}/Managed")
    get_filename_component(samples_runtime_dir "${HELIX_SCRIPTING_RUNTIME}" DIRECTORY)
    get_filename_component(samples_runtime_name "${HELIX_SCRIPTING_RUNTIME}" NAME_WLE)
    # The runtime's config is written beside its assembly by the same build: copied, not depended on.
    set(samples_engine_assemblies "${HELIX_SCRIPTING_RUNTIME}" "${HELIX_CORE_MODULE}" ${HELIX_CORE_MODULE_REFERENCES} "${HELIX_UI_RUNTIME}")
    set(samples_engine_files ${samples_engine_assemblies} "${samples_runtime_dir}/${samples_runtime_name}.runtimeconfig.json")
    file(GLOB samples_script_projects CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Samples/*/Scripts/*.csproj")
    foreach(project IN LISTS samples_script_projects)
        get_filename_component(scripts_dir "${project}" DIRECTORY)
        get_filename_component(sample_dir "${scripts_dir}" DIRECTORY)
        get_filename_component(sample "${sample_dir}" NAME)
        get_filename_component(assembly "${project}" NAME_WLE)
        set(artifacts "${CMAKE_BINARY_DIR}/SampleScripts/${sample}")
        set(dll "${artifacts}/bin/${assembly}/release/${assembly}.dll")
        set(out "${samples_managed_root}/${sample}")
        file(GLOB_RECURSE scripts CONFIGURE_DEPENDS "${scripts_dir}/*.cs")
        list(FILTER scripts EXCLUDE REGEX "/(bin|obj)/")
        add_custom_command(
            OUTPUT "${out}/.staged"
            COMMAND "${HELIX_DOTNET}" build "${project}" -c Release --artifacts-path "${artifacts}" --nologo -v quiet
                    "-p:HelixCoreModule=${HELIX_CORE_MODULE}" "-p:HelixScriptingRuntime=${HELIX_SCRIPTING_RUNTIME}" "-p:HelixUIRuntime=${HELIX_UI_RUNTIME}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${out}"
            COMMAND "${CMAKE_COMMAND}" -E copy "${dll}" ${samples_engine_files} "${out}"
            COMMAND "${CMAKE_COMMAND}" -E touch "${out}/.staged"
            DEPENDS ${scripts} "${project}" ${samples_engine_assemblies}
            COMMENT "Building the C# scripts of the sample ${sample}")
        add_custom_target(helix_samples_scripts_${sample} ALL DEPENDS "${out}/.staged")
        add_dependencies(helix_samples_scripts_${sample} helix_ui_runtime helix_core_module helix_scripting_runtime)
        add_dependencies(helix_samples helix_samples_scripts_${sample})
    endforeach()

    # The shader assemblies beside the launcher: the core's library and shaders, both pipelines', the UI's.
    set(shaders_out "${CMAKE_BINARY_DIR}/Shaders")
    set(shader_assemblies "${HELIX_RENDER_PIPELINES_CORE_SHADER_LIBRARY}" "${HELIX_RENDER_PIPELINES_CORE_SHADERS}"
        "${HELIX_RENDER_PIPELINES_STANDARD_SHADERS}" "${HELIX_RENDER_PIPELINES_PHYSICAL_SHADERS}" "${HELIX_UI_SHADERS}")
    add_custom_command(
        OUTPUT "${shaders_out}/.staged"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${shaders_out}"
        COMMAND "${CMAKE_COMMAND}" -E copy ${shader_assemblies} "${shaders_out}"
        COMMAND "${CMAKE_COMMAND}" -E touch "${shaders_out}/.staged"
        DEPENDS ${shader_assemblies}
        COMMENT "Staging the samples' shader assemblies")
    add_custom_target(helix_samples_shaders ALL DEPENDS "${shaders_out}/.staged")
    add_dependencies(helix_samples_shaders helix_render_pipelines_core_shaders helix_render_pipelines_standard_shaders
                     helix_render_pipelines_physical_shaders helix_ui_shaders)
    add_dependencies(helix_samples helix_samples_shaders)

    # The golden images: each sample on each pipeline in batch mode, its screenshot against Tests/Golden.
    if(HELIX_BUILD_TESTS AND PROJECT_IS_TOP_LEVEL)
        helix_add_executable(helix_samples_compare SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Tools/CompareImages/CompareImages.cpp" DEPS Helix::Image Helix::Platform)
        set_target_properties(helix_samples_compare PROPERTIES OUTPUT_NAME CompareImages)
        set(runner "${CMAKE_CURRENT_SOURCE_DIR}/Tests/RunSample.cmake")
        # Samples drawn with the machine's fonts (MACHINE_FONTS) run everywhere but compare their screenshots only
        # where the goldens were made (not on CI, whose fonts differ).
        set(machine_font_samples UIText UIRender UIInput UIStyles UIElements UIControls UICollections UIBinding UINodeGraph UIImmediate)
        foreach(sample IN ITEMS UILayout UIText UIRender UIInput UIStyles UIElements UIControls UICollections UIBinding UINodeGraph UIImmediate)
            set(machine_fonts 0)
            if(sample IN_LIST machine_font_samples)
                set(machine_fonts 1)
            endif()
            foreach(pipeline IN ITEMS standard physical none)
                add_test(NAME helix_samples_${sample}_${pipeline}
                         COMMAND "${CMAKE_COMMAND}" "-DLAUNCHER=$<TARGET_FILE:helix_samples>" "-DCOMPARE=$<TARGET_FILE:helix_samples_compare>"
                                 "-DSAMPLE=${sample}" "-DPIPELINE=${pipeline}" "-DMACHINE_FONTS=${machine_fonts}"
                                 "-DGOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/Tests/Golden/${sample}-${pipeline}.png"
                                 "-DOUT=${CMAKE_CURRENT_BINARY_DIR}/shots/${sample}-${pipeline}.png" -P "${runner}")
                # The .NET runtime (a sample's C# scripts) is never unloaded: LeakSanitizer is told about its modules.
                if(HELIX_ENABLE_ASAN AND HELIX_PLATFORM STREQUAL "linux")
                    helix_dependency_dir(samples_scripting_dir Scripting)
                    set_tests_properties(helix_samples_${sample}_${pipeline} PROPERTIES
                                         ENVIRONMENT "LSAN_OPTIONS=suppressions=${samples_scripting_dir}/Tests/Support/LeakSuppressions.txt")
                endif()
            endforeach()
        endforeach()
    endif()
endif()
