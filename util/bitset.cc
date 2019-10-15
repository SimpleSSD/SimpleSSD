// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "util/bitset.hh"

#include <cstdlib>
#include <cstring>

#include "util/algorithm.hh"

namespace SimpleSSD {

Bitset::Bitset(ObjectData &o)
    : Object(o), data(nullptr), dataSize(0), allocSize(0) {}

Bitset::Bitset(ObjectData &o, uint32_t size) : Bitset(o) {
  if (size > 0) {
    dataSize = size;
    allocSize = DIVCEIL(dataSize, 8);
    data = (uint8_t *)calloc(allocSize, 1);
  }
}

Bitset::Bitset(Bitset &&rhs) noexcept : Object(rhs.object) {
  *this = std::move(rhs);
}

Bitset::~Bitset() {
  free(data);

  data = nullptr;
  dataSize = 0;
  allocSize = 0;
}

bool Bitset::test(uint32_t idx) noexcept {
  return data[idx / 8] & (0x01 << (idx % 8));
}

bool Bitset::test(uint32_t idx) const noexcept {
  return data[idx / 8] & (0x01 << (idx % 8));
}

bool Bitset::all() noexcept {
  uint8_t ret = 0xFF;
  uint8_t mask = 0xFF << (dataSize + 8 - allocSize * 8);

  for (uint32_t i = 0; i < allocSize - 1; i++) {
    ret &= data[i];
  }

  ret &= data[allocSize - 1] | mask;

  return ret == 0xFF;
}

bool Bitset::all() const noexcept {
  uint8_t ret = 0xFF;
  uint8_t mask = 0xFF << (dataSize + 8 - allocSize * 8);

  for (uint32_t i = 0; i < allocSize - 1; i++) {
    ret &= data[i];
  }

  ret &= data[allocSize - 1] | mask;

  return ret == 0xFF;
}

bool Bitset::any() noexcept {
  return !none();
}

bool Bitset::any() const noexcept {
  return !none();
}

bool Bitset::none() noexcept {
  uint8_t ret = 0x00;

  for (uint32_t i = 0; i < allocSize; i++) {
    ret |= data[i];
  }

  return ret == 0x00;
}

bool Bitset::none() const noexcept {
  uint8_t ret = 0x00;

  for (uint32_t i = 0; i < allocSize; i++) {
    ret |= data[i];
  }

  return ret == 0x00;
}

uint32_t Bitset::count() noexcept {
  uint32_t count = 0;

  for (uint32_t i = 0; i < allocSize; i++) {
    count += popcount16(data[i]);
  }

  return count;
}

uint32_t Bitset::count() const noexcept {
  uint32_t count = 0;

  for (uint32_t i = 0; i < allocSize; i++) {
    count += popcount16(data[i]);
  }

  return count;
}

uint32_t Bitset::size() noexcept {
  return dataSize;
}

uint32_t Bitset::size() const noexcept {
  return dataSize;
}

void Bitset::set() noexcept {
  uint8_t mask = 0xFF >> (allocSize * 8 - dataSize);

  for (uint32_t i = 0; i < allocSize - 1; i++) {
    data[i] = 0xFF;
  }

  data[allocSize - 1] = mask;
}

void Bitset::set(uint32_t idx, bool value) noexcept {
  data[idx / 8] &= ~(0x01 << (idx % 8));

  if (value) {
    data[idx / 8] = data[idx / 8] | (0x01 << (idx % 8));
  }
}

void Bitset::reset() noexcept {
  for (uint32_t i = 0; i < allocSize; i++) {
    data[i] = 0x00;
  }
}

void Bitset::reset(uint32_t idx) noexcept {
  data[idx / 8] &= ~(0x01 << (idx % 8));
}

void Bitset::flip() noexcept {
  uint8_t mask = 0xFF >> (allocSize * 8 - dataSize);

  for (uint32_t i = 0; i < allocSize; i++) {
    data[i] = ~data[i];
  }

  data[allocSize - 1] &= mask;
}

void Bitset::flip(uint32_t idx) noexcept {
  data[idx / 8] = (~data[idx / 8] & (0x01 << (idx % 8))) |
                  (data[idx / 8] & ~(0x01 << (idx % 8)));
}

bool Bitset::operator[](uint32_t idx) noexcept {
  return test(idx);
}

bool Bitset::operator[](uint32_t idx) const noexcept {
  return test(idx);
}

Bitset &Bitset::operator&=(const Bitset &rhs) {
  panic_if(dataSize != rhs.dataSize, "Size does not match");

  for (uint32_t i = 0; i < allocSize; i++) {
    data[i] &= rhs.data[i];
  }

  return *this;
}

Bitset &Bitset::operator|=(const Bitset &rhs) {
  panic_if(dataSize != rhs.dataSize, "Size does not match");

  for (uint32_t i = 0; i < allocSize; i++) {
    data[i] |= rhs.data[i];
  }

  return *this;
}

Bitset &Bitset::operator^=(const Bitset &rhs) {
  panic_if(dataSize != rhs.dataSize, "Size does not match");

  for (uint32_t i = 0; i < allocSize; i++) {
    data[i] ^= rhs.data[i];
  }

  return *this;
}

Bitset &Bitset::operator=(Bitset &&rhs) {
  if (this != &rhs) {
    free(this->data);

    this->data = std::exchange(rhs.data, nullptr);
    this->dataSize = std::exchange(rhs.dataSize, 0);
    this->allocSize = std::exchange(rhs.allocSize, 0);
  }

  return *this;
}

void Bitset::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Bitset::getStatValues(std::vector<double> &) noexcept {}

void Bitset::resetStatValues() noexcept {}

void Bitset::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, dataSize);
  BACKUP_SCALAR(out, allocSize);

  if (allocSize > 0) {
    BACKUP_BLOB(out, data, allocSize);
  }
}

void Bitset::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, dataSize);
  RESTORE_SCALAR(in, allocSize);

  if (allocSize > 0) {
    RESTORE_BLOB(in, data, allocSize);
  }
}

}  // namespace SimpleSSD