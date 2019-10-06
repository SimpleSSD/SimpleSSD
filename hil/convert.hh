// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_CONVERT_HH__
#define __SIMPLESSD_HIL_CONVERT_HH__

#include <cinttypes>
#include <functional>
#include <type_traits>

#include "sim/object.hh"
#include "util/algorithm.hh"

namespace SimpleSSD::HIL {

/**
 * \brief Convert LBA -> LPN
 *
 * nlb must NOT zero.
 *
 * \param[in]  slba       Starting LBA
 * \param[in]  nlb        The number of logical blocks
 * \param[out] slpn       Starting LPN
 * \param[out] nlp        The number of logical pages
 * \param[out] skipFirst  The number of bytes to ignore at the beginning
 * \param[out] skipLast   The number of bytes to ignore at the end
 */
using ConvertFunction = std::function<void(uint64_t, uint64_t, uint64_t &,
                                           uint64_t &, uint32_t *, uint32_t *)>;

/**
 * \brief Convert class
 *
 * This class helps to convert LBA (logical block address) to LPN address.
 *
 * LBA is always uint64_t - NVMe uses 8byte LBA.
 * LPN can have various size. (See SimpleSSD::HIL::HIL class)
 *
 * Both LBA and LPN must power of 2. (popcount == 1)
 */
class Convert : public Object {
 private:
  uint64_t lpnOrder;  //!< Order of LPN size. Fixed in one simulation session.
  uint64_t lbaOrder;  //!< Order of LBA size. Can be modified.
  int16_t shift;
  uint64_t mask;

 public:
  Convert(ObjectData &, uint64_t, uint64_t);
  Convert(const Convert &) = delete;
  Convert(Convert &&) noexcept = default;

  Convert &operator=(const Convert &) = delete;
  Convert &operator=(Convert &&) noexcept = default;

  ConvertFunction getConvertion();

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &out) noexcept override;
  void restoreCheckpoint(std::istream &in) noexcept override;
};

}  // namespace SimpleSSD::HIL

#endif
