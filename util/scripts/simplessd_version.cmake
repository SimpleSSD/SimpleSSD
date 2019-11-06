# SPDX-License-Identifier: GPL-3.0-or-later
#
# Copyright (C) 2019 CAMELab
#
# Author: Donghyun Gouk <kukdh1@camelab.org>
#

cmake_minimum_required(VERSION 3.10)

# Get git version info
find_package(Git QUIET)
if (GIT_FOUND AND EXISTS "${PROJECT_DIRECTORY}/.git")
  execute_process(COMMAND sh -c "git diff --quiet --exit-code || echo +"
    OUTPUT_VARIABLE GIT_CODE_MODIFIED
    OUTPUT_STRIP_TRAILING_WHITESPACE
    WORKING_DIRECTORY ${PROJECT_DIRECTORY})
  execute_process(COMMAND git describe --tags --abbre=8
    OUTPUT_VARIABLE GIT_TAG
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
    WORKING_DIRECTORY ${PROJECT_DIRECTORY})

  string(STRIP "${GIT_TAG}" GIT_TAG)

  if ("${GIT_TAG}" STREQUAL "")
    set(VERSION_STRING "2.1-unknown")
  else ()
    set(VERSION_STRING "${GIT_TAG}${GIT_CODE_MODIFIED}")
  endif ()
else ()
  set(VERSION_STRING "2.1-unknown")
endif ()

message("-- Using version string: ${VERSION_STRING}")

configure_file(${INPUT_VERSION_FILE} ${OUTPUT_VERSION_FILE})

if (NOT _MAKE_VERSION)
  add_custom_target(${VERSION_TARGET}
    ALL
    DEPENDS ${INPUT_VERSION_FILE}
    BYPRODUCTS ${OUTPUT_VERSION_FILE}
    COMMAND
      ${CMAKE_COMMAND}
      -D_MAKE_VERSION=TRUE
      -DINPUT_VERSION_FILE=${INPUT_VERSION_FILE}
      -DOUTPUT_VERSION_FILE=${OUTPUT_VERSION_FILE}
      -DPROJECT_DIRECTORY=${PROJECT_DIRECTORY}
      -P ${CMAKE_CURRENT_LIST_FILE}
  )
endif ()
