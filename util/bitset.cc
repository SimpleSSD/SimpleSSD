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

#include "util/bitset.hh"

#include "util/algorithm.hh"

namespace SimpleSSD {

Bitset::Bitset(uint32_t size) : dataSize(size) {
  if (dataSize == 0) {
    panic("Invalid size of Bitset");
  }

  allocSize = DIVCEIL(dataSize, 8);
  data.resize(allocSize);
}

void Bitset::boundCheck(uint32_t idx) {
  if (idx >= dataSize) {
    panic("Index out of range");
  }
}

bool Bitset::test(uint32_t idx) {
  boundCheck(idx);

  return data[idx / 8] & (0x01 << (idx % 8));
}

bool Bitset::all() {
  uint8_t ret = 0xFF;
  uint8_t mask = 0xFF << (dataSize + 8 - allocSize * 8);

  for (uint32_t i = 0; i < allocSize - 1; i++) {
    ret &= data[i];
  }

  ret &= data[allocSize - 1] | mask;

  return ret == 0xFF;
}

bool Bitset::any() {
  return !none();
}

bool Bitset::none() {
  uint8_t ret = 0x00;

  for (uint32_t i = 0; i < allocSize; i++) {
    ret |= data[i];
  }

  return ret == 0x00;
}

uint32_t Bitset::count() {
  uint32_t count = 0;

  for (uint32_t i = 0; i < allocSize; i++) {
    count += popcount(data[i]);
  }

  return count;
}

uint32_t Bitset::size() {
  return dataSize;
}

void Bitset::set() {
  uint8_t mask = 0xFF >> (allocSize * 8 - dataSize);

  for (uint32_t i = 0; i < allocSize - 1; i++) {
    data[i] = 0xFF;
  }

  data[allocSize - 1] = mask;
}

void Bitset::set(uint32_t idx, bool value) {
  boundCheck(idx);

  data[idx / 8] &= ~(0x01 << (idx % 8));

  if (value) {
    data[idx / 8] = data[idx / 8] | (0x01 << (idx % 8));
  }
}

void Bitset::reset() {
  for (uint32_t i = 0; i < allocSize; i++) {
    data[i] = 0x00;
  }
}

void Bitset::reset(uint32_t idx) {
  boundCheck(idx);

  data[idx / 8] &= ~(0x01 << (idx % 8));
}

void Bitset::flip() {
  uint8_t mask = 0xFF >> (allocSize * 8 - dataSize);

  for (uint32_t i = 0; i < allocSize; i++) {
    data[i] = ~data[i];
  }

  data[allocSize - 1] &= mask;
}

void Bitset::flip(uint32_t idx) {
  boundCheck(idx);

  data[idx / 8] = (~data[idx / 8] & (0x01 << (idx % 8))) |
                  (data[idx / 8] & ~(0x01 << (idx % 8)));
}

bool Bitset::operator[](uint32_t idx) {
  return test(idx);
}

Bitset &Bitset::operator&=(const Bitset &rhs) {
  if (dataSize != rhs.dataSize) {
    panic("Size does not match");
  }

  for (uint32_t i = 0; i < allocSize; i++) {
    data[i] &= rhs.data[i];
  }

  return *this;
}

Bitset &Bitset::operator|=(const Bitset &rhs) {
  if (dataSize != rhs.dataSize) {
    panic("Size does not match");
  }

  for (uint32_t i = 0; i < allocSize; i++) {
    data[i] |= rhs.data[i];
  }

  return *this;
}

Bitset &Bitset::operator^=(const Bitset &rhs) {
  if (dataSize != rhs.dataSize) {
    panic("Size does not match");
  }

  for (uint32_t i = 0; i < allocSize; i++) {
    data[i] ^= rhs.data[i];
  }

  return *this;
}

Bitset Bitset::operator~() const {
  Bitset ret(*this);

  ret.flip();

  return ret;
}

}  // namespace SimpleSSD
