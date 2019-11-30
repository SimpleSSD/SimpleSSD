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
 * \brief Convert byteoffset -> LPN
 *
 * length must NOT zero.
 *
 * \param[in]  offset     Starting byte offset
 * \param[in]  length     Length in bytes
 * \param[out] slpn       Starting LPN
 * \param[out] nlp        The number of logical pages
 * \param[out] skipFirst  The number of bytes to ignore at the beginning
 * \param[out] skipLast   The number of bytes to ignore at the end
 */
using ConvertFunction = std::function<void(uint64_t, uint32_t, LPN &,
                                           uint32_t &, uint32_t &, uint32_t &)>;

/**
 * \brief Convert class
 *
 * This class helps to convert byte offset to LPN address.
 *
 * LPN must power of 2. (popcount == 1)
 */
class Convert : public Object {
 private:
  uint64_t lpnOrder;  //!< Order of LPN size. Fixed in one simulation session.
  uint64_t mask;

 public:
  Convert(ObjectData &, uint32_t);

  ConvertFunction getConvertion();

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL

#endif
