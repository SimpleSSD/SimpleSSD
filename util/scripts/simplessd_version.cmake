# Copyright (C) 2017 CAMELab
#
# This file is part of SimpleSSD.
#
# SimpleSSD is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# SimpleSSD is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with SimpleSSD.  If not, see <http://www.gnu.org/licenses/>.

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
