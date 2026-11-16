# Runs a sample in batch mode on a pipeline and compares its screenshot with the golden image (cmake -P):
#   -DLAUNCHER=<HelixSamples> -DCOMPARE=<CompareImages> -DSAMPLE=<name> -DPIPELINE=<name> -DGOLDEN=<png> -DOUT=<png>
#   [-DGRAPHICS=<a player argument choosing the device, e.g. -force-glcore>] [-DDEVICE=<adapter index>]
#   [-DMACHINE_FONTS=1: drawn with the machine's fonts, compared only off CI]
# The launcher must end with 0 and the images must match (HELIX_UPDATE_GOLDEN=1 writes the golden image).
cmake_minimum_required(VERSION 3.28)

get_filename_component(out_dir "${OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${out_dir}")
file(REMOVE "${OUT}")
set(log "${OUT}.log")
set(arguments -batchmode -frames 8 -screen-width 960 -screen-height 540 -pipeline "${PIPELINE}" -sample "${SAMPLE}" -screenshot "${OUT}" -logFile "${log}")
if(GRAPHICS)
    list(APPEND arguments ${GRAPHICS})
endif()
if(DEFINED DEVICE)
    list(APPEND arguments -force-device-index ${DEVICE})
endif()
execute_process(COMMAND "${LAUNCHER}" ${arguments} RESULT_VARIABLE code OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 300)
if(NOT code EQUAL 0)
    if(EXISTS "${log}")
        file(READ "${log}" text)
    endif()
    message(FATAL_ERROR "${SAMPLE} on ${PIPELINE} ended with ${code}:\n${output}\n${errors}\n${text}")
endif()
if(MACHINE_FONTS AND DEFINED ENV{CI})
    message(STATUS "${SAMPLE} on ${PIPELINE} ran; its screenshot depends on the machine's fonts and is not compared on CI.")
    return()
endif()
execute_process(COMMAND "${COMPARE}" "${GOLDEN}" "${OUT}" RESULT_VARIABLE compared OUTPUT_VARIABLE report)
message(STATUS "${SAMPLE} on ${PIPELINE}: ${report}")
if(NOT compared EQUAL 0)
    message(FATAL_ERROR "${SAMPLE} on ${PIPELINE} does not match ${GOLDEN} (the screenshot is ${OUT}).")
endif()
