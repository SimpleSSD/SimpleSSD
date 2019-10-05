// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/convert.hh"

namespace SimpleSSD::HIL {

Convert::Convert(ObjectData &o, uint64_t lpn, uint64_t lba)
    : Object(o) {
  panic_if(popcount64((uint64_t)lpn) != 1, "Invalid logical page size.");
  panic_if(popcount64(lba) != 1, "Invalid logical block size.");

  lpnOrder = ffs64((uint64_t)lpn) - 1;
  lbaOrder = ffs64(lba) - 1;
  shift = lpnOrder - lbaOrder;
  mask = (1ull << (shift >= 0 ? shift : -shift)) - 1;
}

ConvertFunction Convert::getConvertion() {
  if (shift > 0) {
    // LBASize < LPNSize
    return [shift = this->shift, mask = this->mask](
               uint64_t slba, uint64_t nlb, uint64_t &slpn, uint64_t &nlp,
               uint32_t *skipFirst, uint32_t *skipLast) {
      slpn = slba >> shift;
      nlp = ((slba + nlb - 1) >> shift) + 1 - slpn;

      if (skipFirst) {
        *skipFirst = slba & mask;
      }
      if (skipLast) {
        *skipLast = ((slpn + nlp) << 3) - slpn - nlb;
      }
    };
  }
  else if (shift == 0) {
    // LBASize == LPNSize
    return [shift = this->shift, mask = this->mask](
               uint64_t slba, uint64_t nlb, uint64_t &slpn, uint64_t &nlp,
               uint32_t *skipFirst, uint32_t *skipLast) {
      slpn = slba;
      nlp = nlb;

      if (skipFirst) {
        *skipFirst = 0;
      }
      if (skipLast) {
        *skipLast = 0;
      }
    };
  }
  else {
    // LBASize > LPNSize
    return [shift = -this->shift, mask = this->mask](
               uint64_t slba, uint64_t nlb, uint64_t &slpn, uint64_t &nlp,
               uint32_t *skipFirst, uint32_t *skipLast) {
      slpn = slba << shift;
      nlp = nlb << shift;

      if (skipFirst) {
        *skipFirst = 0;
      }
      if (skipLast) {
        *skipLast = 0;
      }
    };
  }
}

void Convert::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Convert::getStatValues(std::vector<double> &) noexcept {}

void Convert::resetStatValues() noexcept {}

void Convert::createCheckpoint(std::ostream &out) noexcept {
  BACKUP_SCALAR(out, lpnOrder);
  BACKUP_SCALAR(out, lbaOrder);
  BACKUP_SCALAR(out, shift);
  BACKUP_SCALAR(out, mask);
}

void Convert::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, lpnOrder);
  RESTORE_SCALAR(in, lbaOrder);
  RESTORE_SCALAR(in, shift);
  RESTORE_SCALAR(in, mask);
}

}  // namespace SimpleSSD::HIL
