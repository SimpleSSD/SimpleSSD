// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/convert.hh"

namespace SimpleSSD::HIL {

Convert::Convert(ObjectData &o, uint64_t lpn, uint64_t lba) : Object(o) {
  panic_if(popcount64((uint64_t)lpn) != 1, "Invalid logical page size.");
  panic_if(popcount64(lba) != 1, "Invalid logical block size.");

  lpnOrder = ctz64((uint64_t)lpn);
  lbaOrder = ctz64(lba);
  shift = (int16_t)((int64_t)lpnOrder - (int64_t)lbaOrder);
  mask = (1ull << (shift >= 0 ? shift : -shift)) - 1;
}

ConvertFunction Convert::getConvertion() {
  if (shift > 0) {
    // LBASize < LPNSize
    return [shift = this->shift, mask = this->mask, order = lbaOrder](
               uint64_t slba, uint32_t nlb, uint64_t &slpn, uint32_t &nlp,
               uint32_t *skipFirst, uint32_t *skipLast) {
      slpn = slba >> shift;
      nlp = (uint32_t)(((slba + nlb - 1) >> shift) + 1 - slpn);

      if (skipFirst) {
        *skipFirst = (uint32_t)((slba & mask) << order);
      }
      if (skipLast) {
        *skipLast = (uint32_t)((((slpn + nlp) << shift) - slba - nlb) << order);
      }
    };
  }
  else if (shift == 0) {
    // LBASize == LPNSize
    return [](uint64_t slba, uint32_t nlb, uint64_t &slpn, uint32_t &nlp,
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
    return [shift = -this->shift](uint64_t slba, uint32_t nlb, uint64_t &slpn,
                                  uint32_t &nlp, uint32_t *skipFirst,
                                  uint32_t *skipLast) {
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

void Convert::createCheckpoint(std::ostream &out) const noexcept {
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
