// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_UTIL_DISK__
#define __SIMPLESSD_UTIL_DISK__

#include <cinttypes>
#include <fstream>
#include <string>
#include <map>
#include <vector>

#include "sim/object.hh"

namespace SimpleSSD {

class Disk : public Object {
 protected:
  std::string filename;
  uint64_t diskSize;
  uint32_t sectorSize;

  std::fstream disk;

 public:
  Disk(ObjectData &);
  Disk(const Disk &) = delete;
  Disk(Disk &&) noexcept = default;
  virtual ~Disk();

  Disk &operator=(const Disk &) = delete;
  Disk &operator=(Disk &&) noexcept = default;

  virtual uint64_t open(std::string, uint64_t, uint32_t) noexcept;
  virtual void close() noexcept;

  virtual uint16_t read(uint64_t, uint16_t, uint8_t *) noexcept;
  virtual uint16_t write(uint64_t, uint16_t, uint8_t *) noexcept;
  virtual uint16_t erase(uint64_t, uint16_t) noexcept;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

class CoWDisk : public Disk {
 protected:
  std::map<uint64_t, std::vector<uint8_t>> table;

 public:
  CoWDisk(ObjectData &);
  ~CoWDisk();

  void close() noexcept override;

  uint16_t read(uint64_t, uint16_t, uint8_t *) noexcept override;
  uint16_t write(uint64_t, uint16_t, uint8_t *) noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

class MemDisk : public CoWDisk {
 public:
  MemDisk(ObjectData &);
  ~MemDisk();

  uint64_t open(std::string, uint64_t, uint32_t) noexcept override;
  void close() noexcept override;

  uint16_t read(uint64_t, uint16_t, uint8_t *) noexcept override;
  uint16_t write(uint64_t, uint16_t, uint8_t *) noexcept override;
  uint16_t erase(uint64_t, uint16_t) noexcept override;
};

}  // namespace SimpleSSD

#endif
