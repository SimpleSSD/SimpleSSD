// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __HIL_CONVERT_HH__
#define __HIL_CONVERT_HH__

#include <cinttypes>
#include <functional>
#include <type_traits>

#include "sim/object.hh"
#include "util/algorithm.hh"

namespace SimpleSSD::HIL {

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
  Convert(ObjectData &o, uint64_t lpn)
      : Object(o), lbaOrder(0), shift(0), mask(0) {
    panic_if(popcount(lpn) != 1, "Invalid logical page size.");

    lpnOrder = fastlog2(lpn);
  }

  Convert(const Convert &) = delete;
  Convert(Convert &&) noexcept = default;

  Convert &operator=(const Convert &) = delete;
  Convert &operator=(Convert &&) noexcept = default;

  void setLBASize(uint64_t lba) {
    panic_if(popcount(lba) != 1, "Invalid logical block size.");

    lbaOrder = fastlog2(lba);
    shift = lpnOrder - lbaOrder;
    mask = (1ull << (shift >= 0 ? shift : -shift)) - 1;
  }

  uint64_t getLBASize() { return 1ull << lbaOrder; }

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
  template <class LPN, std::enable_if_t<std::is_unsigned_v<LPN>, LPN> = 0>
  using ConvertFunction =
      std::function<void(uint64_t, uint64_t, LPN, LPN, uint32_t *, uint32_t *)>;

  template <class LPN, std::enable_if_t<std::is_unsigned_v<LPN>, LPN> = 0>
  ConvertFunction<LPN> getConvertion() {
    if (shift > 0) {
      // LBASize < LPNSize
      return [shift, mask](uint64_t slba, uint64_t nlb, LPN &slpn, LPN &nlp,
                           uint32_t *skipFirst, uint32_t *skipLast) {
        slpn = slba >> shift;
        nlp = ((slba + nlb - 1) >> shift) + 1 - slpn;

        if (skipFirst) {
          *skipFirst = slba & mask;
        }
        if (skipLast) {
          *skipLast = ((slpn + nlp) << 3) - slbn - nlb;
        }
      };
    }
    else if (shift == 0) {
      // LBASize == LPNSize
      return [shift, mask](uint64_t slba, uint64_t nlb, LPN &slpn, LPN &nlp,
                           uint32_t *skipFirst, uint32_t *skipLast) {
        slpn = slba;
        nlp = nlb;

        if (skipFirst) {
          *skipFirst = 0
        }
        if (skipLast) {
          *skipLast = 0;
        }
      };
    }
    else {
      // LBASize > LPNSize
      return [shift = -shift, mask](uint64_t slba, uint64_t nlb, LPN &slpn,
                                    LPN &nlp, uint32_t *skipFirst,
                                    uint32_t *skipLast) {
        slpn = slba << shift;
        nlp = nlb << shift;

        if (skipFirst) {
          *skipFirst = 0
        }
        if (skipLast) {
          *skipLast = 0;
        }
      };
    }
  }

  void getStatList(std::vector<Stat> &, std::string) noexcept override {}

  void getStatValues(std::vector<double> &) noexcept override {}

  void resetStatValues() noexcept override {}

  void createCheckpoint(std::ostream &out) noexcept override {
    BACKUP_SCALAR(out, lpnOrder);
    BACKUP_SCALAR(out, lbaOrder);
    BACKUP_SCALAR(out, shift);
    BACKUP_SCALAR(out, mask);
  }

  void restoreCheckpoint(std::istream &in) noexcept override {
    RESTORE_SCALAR(in, lpnOrder);
    RESTORE_SCALAR(in, lbaOrder);
    RESTORE_SCALAR(in, shift);
    RESTORE_SCALAR(in, mask);
  }
};

}  // namespace SimpleSSD::HIL

#endif
