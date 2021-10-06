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

#ifndef __FILENAME__
#define __FILENAME__ __FILE__
#endif

#ifdef _MSC_VER
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

namespace SimpleSSD {

#define BACKUP_SCALAR(os, value)                                               \
  { write_checkpoint(os, sizeof(value), (void *)&value); }

#define BACKUP_BLOB(os, data, length)                                          \
  { write_checkpoint(os, (uint32_t)(length), (void *)data); }

#define BACKUP_BLOB64(os, data, length)                                        \
  { write_checkpoint64(os, (uint64_t)(length), (void *)data); }

#define BACKUP_EVENT(os, eid) BACKUP_SCALAR(os, eid)

#define BACKUP_DMATAG(os, tag) BACKUP_SCALAR(os, tag)

#define BACKUP_STL(os, container, iter, expr)                                  \
  {                                                                            \
    uint64_t size = container.size();                                          \
    BACKUP_SCALAR(os, size);                                                   \
    for (const auto &iter : container) {                                       \
      expr                                                                     \
    }                                                                          \
  }

#define RESTORE_SCALAR(is, value)                                              \
  {                                                                            \
    uint32_t _length = sizeof(value);                                          \
                                                                               \
    if (!read_checkpoint(is, _length, (void *)&value)) {                       \
      std::cerr << __FILENAME__ << ":" << __LINE__ << ": "                     \
                << __PRETTY_FUNCTION__ << std::endl;                           \
                                                                               \
      abort();                                                                 \
    }                                                                          \
  }

#define RESTORE_BLOB(is, data, length)                                         \
  {                                                                            \
    uint32_t _length = (uint32_t)(length);                                     \
                                                                               \
    if (!read_checkpoint(is, _length, (void *)data)) {                         \
      std::cerr << __FILENAME__ << ":" << __LINE__ << ": "                     \
                << __PRETTY_FUNCTION__ << std::endl;                           \
                                                                               \
      abort();                                                                 \
    }                                                                          \
  }

#define RESTORE_BLOB64(is, data, length)                                       \
  {                                                                            \
    uint64_t _length = (uint64_t)(length);                                     \
                                                                               \
    if (!read_checkpoint64(is, _length, (void *)data)) {                       \
      std::cerr << __FILENAME__ << ":" << __LINE__ << ": "                     \
                << __PRETTY_FUNCTION__ << std::endl;                           \
                                                                               \
      abort();                                                                 \
    }                                                                          \
  }

#define RESTORE_EVENT(is, value)                                               \
  {                                                                            \
    RESTORE_SCALAR(is, value);                                                 \
    value = object.cpu->restoreEventID(value);                                 \
  }

#define RESTORE_DMATAG(dmaengine, is, value)                                   \
  {                                                                            \
    RESTORE_SCALAR(is, value);                                                 \
    value = (dmaengine)->restoreDMATag(value);                                 \
  }

#define RESTORE_STL(is, iter, expr)                                            \
  {                                                                            \
    uint64_t size;                                                             \
    RESTORE_SCALAR(is, size);                                                  \
    for (uint64_t iter = 0; iter < size; iter++) {                             \
      expr                                                                     \
    }                                                                          \
  }

#define RESTORE_STL_RESERVE(is, container, iter, expr)                         \
  {                                                                            \
    uint64_t size;                                                             \
    RESTORE_SCALAR(is, size);                                                  \
    container.reserve(size);                                                   \
    for (uint64_t iter = 0; iter < size; iter++) {                             \
      expr                                                                     \
    }                                                                          \
  }

#define RESTORE_STL_RESIZE(is, container, iter, expr)                          \
  {                                                                            \
    uint64_t size;                                                             \
    RESTORE_SCALAR(is, size);                                                  \
    container.resize(size);                                                    \
    for (auto &iter : container) {                                             \
      expr                                                                     \
    }                                                                          \
  }

inline bool write_checkpoint(std::ostream &os, uint32_t length, void *ptr) {
  uint32_t header = 0xFE000000 | (length & 0xFFFFFF);

  if (UNLIKELY(length > 0xFFFFFF)) {
    std::cerr << "Data too long to store." << std::endl;

    return false;
  }

  os.write((char *)&header, 4);
  os.write((char *)ptr, length);

  return true;
}

inline bool write_checkpoint64(std::ostream &os, uint64_t length, void *ptr) {
  uint64_t header =
      (uint64_t)(0xFE00000000000000ull | (length & 0xFFFFFFFFFFFFFFull));

  if (UNLIKELY(length > (uint64_t)0xFFFFFFFFFFFFFFull)) {
    std::cerr << "Data too long to store." << std::endl;

    return false;
  }

  os.write((char *)&header, 8);
  os.write((char *)ptr, length);

  return true;
}

inline bool read_checkpoint(std::istream &is, uint32_t &length,
                            void *ptr = nullptr) {
  uint32_t header;

  is.read((char *)&header, 4);

  if (UNLIKELY((header & 0xFF000000) != 0xFE000000)) {
    std::cerr << "Invalid header encountered at offset " << is.tellg()
              << std::endl;

    return false;
  }

  header &= 0xFFFFFF;

  if (UNLIKELY(length > 0 && header != length)) {
    std::cerr << "Length does not match (expected " << length << ", got "
              << header << ")" << std::endl;

    return false;
  }

  if (length > 0) {
    // We know the data size before read
    is.read((char *)ptr, length);
  }
  else {
    // Return only length and not read data
    length = header;
  }

  return true;
}

inline bool read_checkpoint64(std::istream &is, uint64_t &length,
                              void *ptr = nullptr) {
  uint64_t header;

  is.read((char *)&header, 8);

  if (UNLIKELY((header & 0xFF00000000000000ull) != 0xFE00000000000000ull)) {
    std::cerr << "Invalid header encountered at offset " << is.tellg()
              << std::endl;

    return false;
  }

  header &= (uint64_t)0xFFFFFFFFFFFFFFull;

  if (UNLIKELY(length > 0 && header != length)) {
    std::cerr << "Length does not match (expected " << length << ", got "
              << header << ")" << std::endl;

    return false;
  }

  if (length > 0) {
    // We know the data size before read
    is.read((char *)ptr, length);
  }
  else {
    // Return only length and not read data
    length = header;
  }

  return true;
}

inline std::istream &read_checkpoint_data(std::istream &is, uint32_t length,
                                          void *ptr) {
  // Assume length is validated by preceeding read function
  return is.read((char *)ptr, length);
}

}  // namespace SimpleSSD

#endif
