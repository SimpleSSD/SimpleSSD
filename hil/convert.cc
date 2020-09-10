// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/convert.hh"

namespace SimpleSSD::HIL {

Convert::Convert(ObjectData &o, uint32_t lpn) : Object(o) {
  panic_if(popcount32(lpn) != 1, "Invalid logical page size.");

  lpnOrder = ctz32(lpn);
  mask = (1ull << lpnOrder) - 1;
}

ConvertFunction Convert::getConvertion() {
  return [shift = this->lpnOrder, mask = this->mask](
             uint64_t offset, uint32_t length, LPN &slpn, uint32_t &nlp,
             uint32_t &skipFirst, uint32_t &skipLast) {
    slpn = offset >> shift;
    nlp = (uint32_t)(((offset + length - 1) >> shift) + 1 -
                     static_cast<uint64_t>(slpn));
    skipFirst = (uint32_t)(offset & mask);
    skipLast = (uint32_t)((static_cast<uint64_t>(slpn + nlp) << shift) -
                          offset - length);
  };
}

void Convert::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Convert::getStatValues(std::vector<double> &) noexcept {}

void Convert::resetStatValues() noexcept {}

void Convert::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, lpnOrder);
  BACKUP_SCALAR(out, mask);
}

void Convert::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, lpnOrder);
  RESTORE_SCALAR(in, mask);
}

}  // namespace SimpleSSD::HIL
