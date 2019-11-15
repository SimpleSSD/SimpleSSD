// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_UTIL_PATH_HH__
#define __SIMPLESSD_UTIL_PATH_HH__

#include <cinttypes>
#include <string>

#include "sim/object.hh"

namespace SimpleSSD::Path {

std::string makeOutputFilename(ObjectData &obj, const std::string &filename);
std::string makeOutputFilename(ObjectData &obj, const char *filename);

void *openFileMapping(const char *path, uint64_t *psize, bool create = false,
                      bool cow = true);
void closeFileMapping(void *ptr, uint64_t size);

}  // namespace SimpleSSD::Path

#endif
