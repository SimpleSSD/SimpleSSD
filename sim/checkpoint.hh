// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */
/**
 * \file
 * \brief Helper functions for checkpoints.
 *
 * This file provides helper functions for creating/restoring checkpoints.
 */

#pragma once

#ifndef __SIMPLESSD_SIM_CHECKPOINT_HH__
#define __SIMPLESSD_SIM_CHECKPOINT_HH__

#include <cinttypes>
#include <iostream>

#include "util/algorithm.hh"

namespace SimpleSSD {

#define BACKUP_SCALAR(os, value)                                               \
  { write_checkpoint(os, sizeof(value), (void *)&value); }

#define BACKUP_BLOB(os, data, length)                                          \
  { write_checkpoint(os, length, (void *)data); }

#define RESTORE_SCALAR(is, value)                                              \
  {                                                                            \
    uint32_t length = sizeof(value);                                           \
                                                                               \
    read_checkpoint(is, length, (void *)&value);                               \
  }

#define RESTORE_BLOB(is, data, length)                                         \
  {                                                                            \
    uint32_t _length = length;                                                 \
                                                                               \
    read_checkpoint(is, _length, (void *)data);                                \
  }

inline std::ostream &write_checkpoint(std::ostream &os, uint32_t length,
                                      void *ptr) {
  uint32_t header = 0xFE000000 | (length & 0xFFFFFF);

  if (UNLIKELY(length > 0xFFFFFF)) {
    std::cerr << "Data too long to store." << std::endl;

    abort();
  }

  os.write((char *)&header, 4);
  os.write((char *)ptr, length);

  return os;
}

inline std::istream &read_checkpoint(std::istream &is, uint32_t &length,
                                     void *ptr = nullptr) {
  uint32_t header;

  is.read((char *)&header, 4);

  if (UNLIKELY((header & 0xFF000000) != 0xFE000000)) {
    std::cerr << "Invalid header encountered at offset " << is.tellg()
              << std::endl;

    abort();
  }

  header &= 0xFFFFFF;

  if (UNLIKELY(length > 0 && header != length)) {
    std::cerr << "Length does not match (expected " << length << ", got "
              << header << ")" << std::endl;
  }

  if (length > 0) {
    // We know the data size before read
    is.read((char *)ptr, length);
  }
  else {
    // Return only length and not read data
    length = header;
  }

  return is;
}

inline std::istream &read_checkpoint_data(std::istream &is, uint32_t length,
                                          void *ptr) {
  // Assume length is validated by preceeding read function
  return is.read((char *)ptr, length);
}

}  // namespace SimpleSSD

#endif
