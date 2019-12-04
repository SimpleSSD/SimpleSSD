# SPDX-License-Identifier: GPL-3.0-or-later
#
# Copyright (C) 2019 CAMELab
#
# Author: Donghyun Gouk <kukdh1@camelab.org>
#

# Set version and project name
cmake_minimum_required(VERSION 3.10)
project(simplessd)

# Add options for debug build
option(DEBUG_BUILD "Build SimpleSSD in debug mode." OFF)

# Set cache entry for external library include path
set(DRAMPOWER_SOURCE_DIR "." CACHE PATH "Source directory to DRAMPower library")

# Add options for firmware latency generation
option(FW_LATENCY "Use automatic firmware latency generation using LLVM." OFF)

set(FW_CROSS_COMPILE "" CACHE STRING "Set cross-compiler prefix. E.g.: -DFW_CROSS_COMPILE=arm-none-eabi-")
set(FW_TARGET "" CACHE STRING "Set compile target (clang++ --target=<FW_TARGET>). E.g.: arm-eabi")
set(FW_CPU "" CACHE STRING "Set CPU to use (clang++ -mcpu=<FW_CPU>). E.g.: cortex-r52")

# Set output directory
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Check DRAMPOWER_SOURCE_DIR is valid
if (NOT EXISTS "${DRAMPOWER_SOURCE_DIR}/libdrampower/LibDRAMPower.h")
  message(FATAL_ERROR "Failed to find DRAMPower library. \
Check you specified DRAMPower path by -DDRAMPOWER_SOURCE_DIR.")
endif ()

# Check mcpat is valid
if (NOT EXISTS "${PROJECT_SOURCE_DIR}/lib/mcpat/mcpat.h")
  message(FATAL_ERROR "Failed to find McPAT library. \
Check you initialized submodules.")
endif ()

# Check pugixml is valid
if (NOT EXISTS "${PROJECT_SOURCE_DIR}/lib/pugixml/src/pugixml.hpp")
message(FATAL_ERROR "Failed to find pugixml library. \
Check you initialized submodules.")
endif ()

# Add subproject
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/mcpat)
add_subdirectory(${PROJECT_SOURCE_DIR}/lib/pugixml)

# Set include directories
include_directories(
  ${PROJECT_SOURCE_DIR}
  ${PROJECT_SOURCE_DIR}/lib/mcpat
  ${PROJECT_SOURCE_DIR}/lib/mcpat/cacti
  ${PROJECT_SOURCE_DIR}/lib/pugixml/src
  ${DRAMPOWER_SOURCE_DIR}
)

# Make version
set(INPUT_VERSION_FILE "${PROJECT_SOURCE_DIR}/sim/version.cc.in")
set(OUTPUT_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/version.cc")
set(PROJECT_DIRECTORY "${PROJECT_SOURCE_DIR}")
set(VERSION_TARGET "simplessd_version")

include(${PROJECT_SOURCE_DIR}/util/scripts/simplessd_version.cmake)

# Platform specific settings
if (MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_compile_options(/wd4819)  # Surpress unicode warning

  if (FW_LATENCY)
    message(WARNING "Automatic firmware latency generation is not tested on Windows")
  endif ()
else ()
  add_compile_options(-g)
  add_compile_options(-Wall)
  add_compile_options(-Wextra)
  add_compile_options(-Werror)

  set(CMAKE_CXX_FLAGS "-D__FILENAME__='\"$(subst ${PROJECT_SOURCE_DIR}/,,$(abspath $<))\"' ${CMAKE_CXX_FLAGS}")

  if (DEBUG_BUILD)
    add_definitions(-DSIMPLESSD_DEBUG)
    add_compile_options(-O0)
  else ()
    add_compile_options(-O2)
  endif ()
endif ()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Specify source files
set(SRC_CONFIG
  cpu/config.cc
  mem/config.cc
  hil/config.cc
  icl/config.cc
  ftl/config.cc
  fil/config.cc
)
set(SRC_CPU
  cpu/cpu.cc
)
set(SRC_MEM
  mem/dram/controller.cc
  mem/dram/abstract_dram.cc
  mem/dram/ideal.cc
  mem/dram/lpddr4.cc
  mem/sram/abstract_sram.cc
  mem/sram/sram.cc
)
set(SRC_HIL
  hil/convert.cc
  hil/hil.cc
)
set(SRC_HIL_COMMON
  hil/common/dma_engine.cc
  hil/common/interrupt_manager.cc
)
set(SRC_HIL_NVME
  hil/nvme/controller.cc
  hil/nvme/namespace.cc
  hil/nvme/queue_arbitrator.cc
  hil/nvme/queue.cc
  hil/nvme/subsystem.cc
)
set(SRC_HIL_NVME_COMMAND
  hil/nvme/command/abstract_command.cc
  hil/nvme/command/command.cc
  hil/nvme/command/feature.cc
  hil/nvme/command/log_page.cc
  hil/nvme/command/delete_sq.cc
  hil/nvme/command/create_sq.cc
  hil/nvme/command/get_log_page.cc
  hil/nvme/command/delete_cq.cc
  hil/nvme/command/create_cq.cc
  hil/nvme/command/identify.cc
  hil/nvme/command/abort.cc
  hil/nvme/command/set_feature.cc
  hil/nvme/command/get_feature.cc
  hil/nvme/command/async_event_request.cc
  hil/nvme/command/namespace_management.cc
  hil/nvme/command/namespace_attachment.cc
  hil/nvme/command/format_nvm.cc
  hil/nvme/command/flush.cc
  hil/nvme/command/write.cc
  hil/nvme/command/read.cc
  hil/nvme/command/compare.cc
  hil/nvme/command/dataset_management.cc
)
set(SRC_ICL
  icl/icl.cc
)
set(SRC_FTL
  ftl/ftl.cc
)
set(SRC_FTL_ALLOC
  ftl/allocator/basic_allocator.cc
)
set(SRC_FTL_BASE
  ftl/base/basic_ftl.cc
)
set(SRC_FTL_MAPPING
  ftl/mapping/abstract_mapping.cc
)
set(SRC_FIL
  fil/fil.cc
)
set(SRC_FIL_NVM_PAL
  fil/nvm/pal/convert.cc
  fil/nvm/pal/Latency.cc
  fil/nvm/pal/LatencyMLC.cc
  fil/nvm/pal/LatencySLC.cc
  fil/nvm/pal/LatencyTLC.cc
  fil/nvm/pal/PAL2.cc
  fil/nvm/pal/PAL2_TimeSlot.cc
  fil/nvm/pal/PALStatistics.cc
  fil/nvm/pal/pal_wrapper.cc
)
set(SRC_FIL_SCHEDULER
  fil/scheduler/noop.cc
)
set(SRC_SIM
  sim/base_config.cc
  sim/config_reader.cc
  sim/config.cc
  sim/log.cc
  sim/simplessd.cc
)
set(SRC_UTIL
  util/bitset.cc
  util/fifo.cc
  util/interface.cc
  util/path.cc
  util/sorted_map.cc
)

# Source group for MSVC
SOURCE_GROUP("Source Files\\cpu" FILES ${SRC_CPU})
SOURCE_GROUP("Source Files\\memory" FILES ${SRC_MEM})
SOURCE_GROUP("Source Files\\hil" FILES ${SRC_HIL})
SOURCE_GROUP("Source Files\\hil\\common" FILES ${SRC_HIL_COMMON})
SOURCE_GROUP("Source Files\\hil\\nvme" FILES ${SRC_HIL_NVME})
SOURCE_GROUP("Source Files\\hil\\nvme\\command" FILES ${SRC_HIL_NVME_COMMAND})
SOURCE_GROUP("Source Files\\icl" FILES ${SRC_ICL})
SOURCE_GROUP("Source Files\\ftl" FILES ${SRC_FTL})
SOURCE_GROUP("Source Files\\ftl\\allocator" FILES ${SRC_FTL_ALLOC})
SOURCE_GROUP("Source Files\\ftl\\base" FILES ${SRC_FTL_BASE})
SOURCE_GROUP("Source Files\\ftl\\mapping" FILES ${SRC_FTL_MAPPING})
SOURCE_GROUP("Source Files\\fil" FILES ${SRC_FIL})
SOURCE_GROUP("Source Files\\fil\\nvm\\pal" FILES ${SRC_FIL_NVM_PAL})
SOURCE_GROUP("Source Files\\fil\\scheduler" FILES ${SRC_FIL_SCHEDULER})
SOURCE_GROUP("Source Files\\sim" FILES ${SRC_SIM} ${SRC_CONFIG})
SOURCE_GROUP("Source Files\\util" FILES ${SRC_UTIL})

# All sources with instruction statistics
set(SRCS
  ${SRC_HIL}
  ${SRC_HIL_COMMON}
  ${SRC_HIL_NVME}
  ${SRC_HIL_NVME_COMMAND}
  ${SRC_ICL}
  ${SRC_FTL}
  ${SRC_FTL_ALLOC}
  ${SRC_FTL_BASE}
  ${SRC_FTL_MAPPING}
  ${SRC_FIL}
  ${SRC_FIL_NVM_PAL}
  ${SRC_FIL_SCHEDULER}
)

# All sources without instruction statistics
set(SRCS_EXCLUDE
  ${SRC_CPU}
  ${SRC_MEM}
  ${SRC_SIM}
  ${SRC_UTIL}
  ${SRC_CONFIG}
  ${OUTPUT_VERSION_FILE}
)

if (FW_LATENCY)
  # Check llvm-simplessd is valid
  if (NOT EXISTS "${PROJECT_SOURCE_DIR}/lib/llvm-simplessd/CMakeLists.txt")
    message(FATAL_ERROR "Failed to find llvm-simplessd library. \
    Check you initialized submodules.")
  endif ()

  # Detects CROSS_COMPILE
  if (NOT "${FW_CROSS_COMPILE}" STREQUAL "")
    message("-- Detecting include path from ${FW_CROSS_COMPILE}g++")

    # Get include paths
    execute_process(COMMAND sh -c "${FW_CROSS_COMPILE}g++ -E -Wp,-v -x c++ - < /dev/null"
      RESULT_VARIABLE FW_CROSS_RESULT
      ERROR_VARIABLE FW_CROSS_OUTPUT
      OUTPUT_QUIET
    )

    if (NOT ${FW_CROSS_RESULT} EQUAL 0)
      message(FATAL_ERROR "Command failed with error code: ${FW_CROSS_RESULT}")
    endif ()

    # Parse
    string(REPLACE "\n" ";" FW_CROSS_OUTPUT ${FW_CROSS_OUTPUT})

    set(GOOD 0)
    set(FW_INCLUDE "")

    foreach (LINE IN ITEMS ${FW_CROSS_OUTPUT})
      if ("${LINE}" STREQUAL "End of search list.")
        set(GOOD 0)
      endif ()

      if (${GOOD} EQUAL 1)
        string(STRIP "${LINE}" LINE)
        list(APPEND FW_INCLUDE "-I${LINE}")
      endif ()

      if ("${LINE}" STREQUAL "#include <...> search starts here:")
        set(GOOD 1)
      endif ()
    endforeach ()

    if ("${FW_TARGET}" STREQUAL "")
      message(FATAL_ERROR "You need to specify FW_TARGET if you use FW_CROSS_COMPILE")
    endif ()

    if ("${FW_CPU}" STREQUAL "")
      message(FATAL_ERROR "You need to specify FW_CPU if you use FW_CROSS_COMPILE")
    endif ()
  endif ()

  # Make OBJS list
  set(BUILD_DIRECTORY "${PROJECT_BINARY_DIR}/objects")
  string(REGEX REPLACE "([^;]+)" "${BUILD_DIRECTORY}/\\1.o" OBJS "${SRCS}")

  # Define target
  add_library(simplessd-objects OBJECT IMPORTED)
  set_target_properties(simplessd-objects PROPERTIES IMPORTED_OBJECTS "${OBJS}")

  # We need to build LLVM pass
  add_subdirectory(${PROJECT_SOURCE_DIR}/lib/llvm-simplessd)

  # Make commands
  set(LLVM_PASS "${CMAKE_BINARY_DIR}/libllvm-simplessd.so")
  set(STAT_COLLECTOR "${CMAKE_BINARY_DIR}/inststat-generator")

  list(LENGTH SRCS SRC_LEN)
  math(EXPR SRC_LEN "${SRC_LEN} - 1")

  # Generate clang++ arguments
  set(FW_DEFAULT_ARGS
    "-std=c++17"
    "-g"
    "-c"
    "-emit-llvm"
    "-I${PROJECT_SOURCE_DIR}"
    "-I${DRAMPOWER_SOURCE_DIR}"
  )

  set(FW_FIRST_PASS_ARGS
    "-DEXCLUDE_CPU"
  )

  if (NOT "${FW_TARGET}" STREQUAL "")
    string(FIND ${FW_TARGET} "-" FW_TARGET_POS)
    string(SUBSTRING ${FW_TARGET} 0 ${FW_TARGET_POS} FW_ARCH)

    list(APPEND FW_FIRST_PASS_ARGS "--target=${FW_TARGET}")
    list(APPEND FW_FIRST_PASS_ARGS "-m${FW_ARCH}")
  endif ()

  if (NOT "${FW_CPU}" STREQUAL "")
    set(FW_CPU_ARG "-mcpu=${FW_CPU}")
    list(APPEND FW_FIRST_PASS_ARGS "${FW_CPU_ARG}")
  endif ()

  foreach (IDX RANGE ${SRC_LEN})
    list(GET SRCS ${IDX} SRC)
    list(GET OBJS ${IDX} OBJ)

    get_filename_component(DIR ${OBJ} DIRECTORY)

    add_custom_command(OUTPUT ${OBJ}
      COMMAND mkdir -p ${DIR}
      COMMAND clang++ ${FW_DEFAULT_ARGS} ${FW_FIRST_PASS_ARGS} ${FW_INCLUDE} -o "${OBJ}.bc" ${SRC}
      COMMAND opt --load=${LLVM_PASS} --blockcollector -O2 -o "${OBJ}.opt.bc" "${OBJ}.bc"
      COMMAND llc ${FW_CPU_ARG} -O2 -filetype=asm -o "${OBJ}.bc.S" "${OBJ}.opt.bc"
      COMMAND ${STAT_COLLECTOR} "${OBJ}.bc"
      COMMAND clang++ ${FW_DEFAULT_ARGS} -O2 -o "${OBJ}.bc" ${SRC}
      COMMAND opt --load=${LLVM_PASS} --inststat -O2 -o "${OBJ}.opt.bc" "${OBJ}.bc"
      COMMAND llc -O2 -filetype=obj -o ${OBJ} "${OBJ}.opt.bc"
      DEPENDS ${SRC}
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      COMMENT "Instrumenting file ${SRC}"
    )
  endforeach ()

  # CMake should run custom commands to create simplessd-objects
  add_custom_target(build-objects ALL
    DEPENDS ${OBJS}
  )

  # Make target
  add_library(simplessd STATIC
    ${SRCS_EXCLUDE}
    $<TARGET_OBJECTS:simplessd-objects>
  )

  # Make dependency
  add_dependencies(build-objects llvm-simplessd)
  add_dependencies(simplessd build-objects)
else ()
  add_library(simplessd STATIC ${SRCS} ${SRCS_EXCLUDE})
endif ()

# Force version checking in every build
add_dependencies(simplessd ${VERSION_TARGET})

target_link_libraries(simplessd mcpat pugixml)
