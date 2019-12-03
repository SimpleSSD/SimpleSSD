// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FIL_COMMON_FILE_HH__
#define __SIMPLESSD_FIL_COMMON_FILE_HH__

#include "sim/object.hh"

namespace SimpleSSD::FIL {

/**
 * \brief Definition of abstract backing file
 *
 * This base class provides OS-specific memory-mapped file I/O API.
 */
class BackingFile : public Object {
 protected:
  bool checkDirectory(const char *dirpath);
  bool checkFile(const char *filepath);
  uint8_t *openFileMapping(const char *filepath, uint64_t &length, bool cow);
  void closeFileMapping(uint8_t *pointer, uint64_t length);

 public:
  BackingFile(ObjectData &o) : Object(o) {}
  virtual ~BackingFile() {}

  void getStatList(std::vector<Stat> &, std::string) noexcept override {}
  void getStatValues(std::vector<double> &) noexcept override {}
  void resetStatValues() noexcept override {}
};

/**
 * \brief Definition of backing file class for NAND
 *
 * This class provides NAND flash backing files.
 */
class NANDBackingFile : public BackingFile {
 protected:
  const uint64_t totalBlocks;
  const uint32_t page;
  const uint32_t blockSize;
  const uint32_t pageSize;
  const uint32_t spareSize;

  uint8_t **blockData;

 public:
  NANDBackingFile(ObjectData &);
  virtual ~NANDBackingFile();

  /**
   * \brief Read page
   *
   * Get pointer to page.
   *
   * \param[in] blockID   Block ID
   * \param[in] pageIndex Page index
   * \return Const pointer to data
   */
  const uint8_t *read(uint64_t blockID, uint32_t pageIndex);

  /**
   * \brief Write data
   *
   * Write data to specified page. Buffer must have length greater or equal to
   * physical page size (data size + spare size)
   *
   * \param[in] blockID   Block ID
   * \param[in] pageIndex Page index
   * \param[in] buffer  Pointer to buffer
   */
  void write(uint64_t blockID, uint32_t pageIndex, uint8_t *buffer);

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FIL

#endif
