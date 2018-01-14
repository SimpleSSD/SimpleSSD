/*
 * Copyright (C) 2017 CAMELab
 *
 * This file is part of SimpleSSD.
 *
 * SimpleSSD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SimpleSSD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SimpleSSD.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "util/def.hh"

#include <cstdlib>
#include <iostream>

#include "util/algorithm.hh"

namespace SimpleSSD {

LPNRange::_LPNRange() : slpn(0), nlp(0) {}

LPNRange::_LPNRange(uint64_t s, uint64_t n) : slpn(s), nlp(n) {}

DynamicBitset::DynamicBitset(uint32_t size) : dataSize(size) {
  if (dataSize == 0) {
    Logger::panic("Invalid size of DynamicBitset");
  }

  allocSize = (dataSize - 1) / 8 + 1;
  data.resize(allocSize);
}

void DynamicBitset::boundCheck(uint32_t idx) {
  if (idx >= dataSize) {
    Logger::panic("Index out of range");
  }
}

bool DynamicBitset::test(uint32_t idx) {
  boundCheck(idx);

  return data[idx / 8] & (0x01 << (idx % 8));
}

bool DynamicBitset::all() {
  uint8_t ret = 0xFF;
  uint8_t mask = 0xFF << (dataSize + 8 - allocSize * 8);

  for (uint32_t i = 0; i < allocSize - 1; i++) {
    ret &= data[i];
  }

  ret &= data[allocSize - 1] | mask;

  return ret == 0xFF;
}

bool DynamicBitset::any() {
  return !none();
}

bool DynamicBitset::none() {
  uint8_t ret = 0x00;

  for (uint32_t i = 0; i < allocSize; i++) {
    ret |= data[i];
  }

  return ret == 0x00;
}

uint32_t DynamicBitset::count() {
  uint32_t count = 0;

  for (uint32_t i = 0; i < allocSize; i++) {
    count += popcount(data[i]);
  }

  return count;
}

uint32_t DynamicBitset::size() {
  return dataSize;
}

void DynamicBitset::set() {
  uint8_t mask = 0xFF >> (allocSize * 8 - dataSize);

  for (uint32_t i = 0; i < allocSize - 1; i++) {
    data[i] = 0xFF;
  }

  data[allocSize - 1] = mask;
}

void DynamicBitset::set(uint32_t idx, bool value) {
  boundCheck(idx);

  data[idx / 8] &= ~(0x01 << (idx % 8));

  if (value) {
    data[idx / 8] = data[idx / 8] | (0x01 << (idx % 8));
  }
}

void DynamicBitset::reset() {
  for (uint32_t i = 0; i < allocSize; i++) {
    data[i] = 0x00;
  }
}

void DynamicBitset::reset(uint32_t idx) {
  boundCheck(idx);

  data[idx / 8] &= ~(0x01 << (idx % 8));
}

void DynamicBitset::flip() {
  uint8_t mask = 0xFF >> (allocSize * 8 - dataSize);

  for (uint32_t i = 0; i < allocSize; i++) {
    data[i] = ~data[i];
  }

  data[allocSize - 1] &= mask;
}

void DynamicBitset::flip(uint32_t idx) {
  boundCheck(idx);

  data[idx / 8] = (~data[idx / 8] & (0x01 << (idx % 8))) |
                  (data[idx / 8] & ~(0x01 << (idx % 8)));
}

bool DynamicBitset::operator[](uint32_t idx) {
  return test(idx);
}

void DynamicBitset::print() {
  for (uint32_t i = 0; i < dataSize; i++) {
    printf("%d ", test(i) ? 1 : 0);
  }

  printf("\n");
}

DynamicBitset &DynamicBitset::operator&=(const DynamicBitset &rhs) {
  if (dataSize != rhs.dataSize) {
    Logger::panic("Size does not match");
  }

  for (uint32_t i = 0; i < allocSize; i++) {
    data[i] &= rhs.data[i];
  }

  return *this;
}

DynamicBitset &DynamicBitset::operator|=(const DynamicBitset &rhs) {
  if (dataSize != rhs.dataSize) {
    Logger::panic("Size does not match");
  }

  for (uint32_t i = 0; i < allocSize; i++) {
    data[i] |= rhs.data[i];
  }

  return *this;
}

DynamicBitset &DynamicBitset::operator^=(const DynamicBitset &rhs) {
  if (dataSize != rhs.dataSize) {
    Logger::panic("Size does not match");
  }

  for (uint32_t i = 0; i < allocSize; i++) {
    data[i] ^= rhs.data[i];
  }

  return *this;
}

DynamicBitset DynamicBitset::operator~() const {
  DynamicBitset ret(*this);

  ret.flip();

  return ret;
}

namespace ICL {

Request::_Request() : reqID(0), reqSubID(0), offset(0), length(0) {}

}  // namespace ICL

namespace FTL {

Request::_Request(uint32_t iocount)
    : reqID(0), reqSubID(0), lpn(0), ioFlag(iocount) {}

}  // namespace FTL

namespace PAL {

Request::_Request(uint32_t iocount)
    : reqID(0), reqSubID(0), blockIndex(0), pageIndex(0), ioFlag(iocount) {}

Request::_Request(FTL::Request &r)
    : reqID(r.reqID),
      reqSubID(r.reqSubID),
      blockIndex(0),
      pageIndex(0),
      ioFlag(r.ioFlag) {}

}  // namespace PAL

}  // namespace SimpleSSD
