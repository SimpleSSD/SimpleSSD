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

# Get git version info
find_package(Git)
if (GIT_FOUND)
  execute_process(COMMAND sh -c "git diff --quiet --exit-code || echo +"
    OUTPUT_VARIABLE GIT_CODE_MODIFIED
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process(COMMAND git describe --tags --abbre=0
    OUTPUT_VARIABLE GIT_TAG
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process(COMMAND git log --pretty=format:'%H' -n 1
    OUTPUT_VARIABLE GIT_REV
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  string(STRIP "${GIT_TAG}" GIT_TAG)
  string(SUBSTRING "${GIT_REV}" 1 8 GIT_REV)

  if ("${GIT_TAG}" STREQUAL "")
    execute_process(COMMAND git rev-parse --abbrev-ref HEAD
      OUTPUT_VARIABLE GIT_TAG
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif ()

  set(VERSION_STRING "${GIT_TAG}-${GIT_REV}${GIT_CODE_MODIFIED}")
else ()
  set(VERSION_STRING "2.1-unknown")
  set(GIT_TAG "2.1")
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

# Platform specific settings
if (MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_compile_options(/wd4819)  # Surpress unicode warning
  add_compile_options(/wd4244)  # Surpress conversion warning
  add_compile_options(/wd4267)  # Surpress conversion warning
  add_compile_options(/wd4305)  # Surpress truncation warning
  add_compile_options(/wd4800)  # Surpress int -> boolean waring
else ()
  set(CMAKE_CXX_FLAGS "-g -rdynamic -Wall -Wextra -Werror ${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "-D__FILENAME__='\"$(subst ${PROJECT_SOURCE_DIR}/,,$(abspath $<))\"' ${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "-DSIMPLESSD_FULL='\"${VERSION_STRING}\"' ${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "-DSIMPLESSD_TAG='\"${GIT_TAG}-\"' ${CMAKE_CXX_FLAGS}")

  if (DEBUG_BUILD)
    set(CMAKE_CXX_FLAGS "-O0 ${CMAKE_CXX_FLAGS}")
  else ()
    set(CMAKE_CXX_FLAGS "-O2 ${CMAKE_CXX_FLAGS}")
  endif ()
endif ()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Specify source files
set(SRC_CPU
  cpu/config.cc
)
set(SRC_MEM
  mem/config.cc
  mem/dram/abstract_dram.cc
  mem/dram/simple.cc
  mem/sram/abstract_sram.cc
  mem/sram/sram.cc
)
set(SRC_HIL
  hil/config.cc
  hil/convert.cc
)
set(SRC_HIL_COMMON
  hil/common/dma_engine.cc
  hil/common/interrupt_manager.cc
)
set(SRC_HIL_NVME
  hil/nvme/controller.cc
  hil/nvme/dma_engine.cc
  hil/nvme/namespace.cc
  hil/nvme/queue_arbitrator.cc
  hil/nvme/queue.cc
  hil/nvme/subsystem.cc
)
set(SRC_HIL_NVME_COMMAND
  hil/nvme/command/abstract_command.cc
  hil/nvme/command/identify.cc
)
set(SRC_SIM
  sim/base_config.cc
  sim/config_reader.cc
  sim/config.cc
  sim/log.cc
  sim/simplessd.cc
)
set(SRC_UTIL
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
SOURCE_GROUP("Source Files\\sim" FILES ${SRC_SIM})
SOURCE_GROUP("Source Files\\util" FILES ${SRC_UTIL})

# Define library
add_library(simplessd STATIC
  ${SRC_CPU}
  ${SRC_MEM}
  ${SRC_HIL}
  ${SRC_HIL_COMMON}
  ${SRC_HIL_NVME}
  ${SRC_HIL_NVME_COMMAND}
  ${SRC_SIM}
  ${SRC_UTIL}
)
target_link_libraries(simplessd mcpat pugixml)
