// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FIL_CONVERT_HH__
#define __SIMPLESSD_FIL_CONVERT_HH__

#include <cinttypes>
#include <functional>
#include <type_traits>

#include "SimpleSSD_types.h"
#include "fil/def.hh"
#include "sim/object.hh"
#include "util/algorithm.hh"

namespace SimpleSSD::FIL {

/**
 * \brief Convert PPN -> CPDPBP
 *
 * \param[in]  req  Request
 * \param[out] addr CPDPBP
 */
using ConvertFunction = std::function<void(PPN, ::CPDPBP &)>;

/**
 * \brief Convert class
 *
 * This class helps to convert LBA (logical block address) to LPN address.
 *
 * LBA is always uint64_t - NVMe uses 8byte LBA.
 * LPN can have various size. (See SimpleSSD::HIL class)
 *
 * Both LBA and LPN must power of 2. (popcount == 1)
 */
class Convert : public Object {
 private:
  bool isPowerOfTwo;

  // 2^n
  uint64_t maskChannel;
  uint64_t maskWay;
  uint64_t maskDie;
  uint64_t maskPlane;
  uint64_t maskBlock;
  uint64_t maskPage;

  uint8_t shiftChannel;
  uint8_t shiftWay;
  uint8_t shiftDie;
  uint8_t shiftPlane;
  uint8_t shiftBlock;
  uint8_t shiftPage;

  // Generic
  uint64_t channel;
  uint64_t way;
  uint64_t die;
  uint64_t plane;
  uint64_t block;
  uint64_t page;

 public:
  Convert(ObjectData &);

  ConvertFunction getConvertion();

  void getBlockAlignedPPN(PPN &);
  void increasePage(PPN &);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FIL

#endif
