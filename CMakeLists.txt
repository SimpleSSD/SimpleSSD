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
option(INST_STAT "Use automatic instruction statistic generation using LLVM." OFF)

# Set output directory
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Set cache entry for external library include path
set(DRAMPOWER_SOURCE_DIR "." CACHE PATH "Source directory to DRAMPower library")

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

# check llvm-simplessd is valid
if (INST_STAT AND NOT EXISTS "${PROJECT_SOURCE_DIR}/lib/llvm-simplessd/CMakeLists.txt")
message(FATAL_ERROR "Failed to find llvm-simplessd library. \
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

# Specify source files
set(SRC_CPU
  cpu/config.cc
  cpu/cpu.cc
)
set(SRC_MEM
  mem/config.cc
  mem/dram/abstract_dram.cc
  mem/dram/simple.cc
  mem/dram/gem5.cc
  mem/sram/abstract_sram.cc
  mem/sram/sram.cc
)
set(SRC_HIL
  hil/config.cc
  hil/command_manager.cc
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
  icl/config.cc
  icl/icl.cc
  icl/ring_buffer.cc
)
set(SRC_FTL
  ftl/config.cc
  ftl/ftl.cc
)
set(SRC_FTL_ALLOC
  ftl/allocator/basic_allocator.cc
  ftl/allocator/two_block_allocator.cc
  ftl/allocator/vl_allocator.cc
)
set(SRC_FTL_BASE
  ftl/base/basic_ftl.cc
  ftl/base/vlftl.cc
)
set(SRC_FTL_MAPPING
  ftl/mapping/abstract_mapping.cc
  ftl/mapping/page_level.cc
  ftl/mapping/virtually_linked.cc
)
set(SRC_FIL
  fil/config.cc
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
  util/disk.cc
  util/fifo.cc
  util/interface.cc
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
SOURCE_GROUP("Source Files\\sim" FILES ${SRC_SIM})
SOURCE_GROUP("Source Files\\util" FILES ${SRC_UTIL})

# All sources
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
set(SRC_EXCLUDE
  ${SRC_CPU}
  ${SRC_MEM}
  ${SRC_SIM}
  ${SRC_UTIL}
  ${OUTPUT_VERSION_FILE}
)

# Must be built from object files
if (INST_STAT)
  # Make OBJS list
  set(BUILD_DIRECTORY "${PROJECT_BINARY_DIR}/objects")
  string(REGEX REPLACE "([^;]+)" "${BUILD_DIRECTORY}/\\1.o" OBJS "${SRCS}")

  # Define target
  add_library(simplessd STATIC ${SRC_EXCLUDE})

  # We need to build LLVM pass
  add_subdirectory(${PROJECT_SOURCE_DIR}/lib/llvm-simplessd)

  # Instruction architecture
  set(ARCH_FLAGS "-march=arm")
  set(CPU_FLAGS "-mcpu=cortex-r7")
  set(ATTR_FLAGS "-mattr=r7")

  # Make commands
  set(LLVM_PASS "${CMAKE_BINARY_DIR}/libllvm-simplessd.so")

  list(LENGTH SRCS SRC_LEN)
  math(EXPR SRC_LEN "${SRC_LEN} - 1")

  foreach (IDX RANGE ${SRC_LEN})
    list(GET SRCS ${IDX} SRC)
    list(GET OBJS ${IDX} OBJ)

    get_filename_component(DIR ${OBJ} DIRECTORY)

    add_custom_command(OUTPUT ${OBJ}
      COMMAND mkdir -p ${DIR}
      COMMAND clang++ -std=c++17 -g -c -emit-llvm -DEXCLUDE_CPU -I${PROJECT_SOURCE_DIR} -I${DRAMPOWER_SOURCE_DIR} -o "${OBJ}.bc" ${SRC}
      COMMAND opt ${ARCH_FLAGS} ${CPU_FLAGS} ${ATTR_FLAGS} --load=${LLVM_PASS} --blockcollector -O2 -o "${OBJ}.opt.bc" "${OBJ}.bc"
      COMMAND llc ${ARCH_FLAGS} ${CPU_FLAGS} ${ATTR_FLAGS} -O2 -filetype=asm -o "${OBJ}.S" "${OBJ}.opt.bc"
      COMMAND touch "${OBJ}.bc.inststat.txt"
      COMMAND clang++ -std=c++17 -g -c -emit-llvm -I${PROJECT_SOURCE_DIR} -I${DRAMPOWER_SOURCE_DIR} -o "${OBJ}.bc" ${SRC}
      COMMAND opt --load=${LLVM_PASS} --inststat -O2 -o "${OBJ}.opt.bc" "${OBJ}.bc"
      COMMAND llc -O2 -filetype=obj -o ${OBJ} "${OBJ}.opt.bc"
      DEPENDS ${SRC}
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      COMMENT "Instrumenting file ${SRC}"
    )
  endforeach ()

  # Make target
  add_custom_target(inststat ALL
    DEPENDS ${OBJS}
  )

  # Make dependency
  add_dependencies(inststat llvm-simplessd)
  add_dependencies(simplessd inststat)

  # Link
  target_link_libraries(simplessd ${OBJS})
else ()
  add_library(simplessd STATIC ${SRC_EXCLUDE} ${SRCS})
endif ()

# Force version checking in every build
add_dependencies(simplessd ${VERSION_TARGET})

target_link_libraries(simplessd mcpat pugixml)
