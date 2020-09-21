// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "util/bitset.hh"

#include <cstdlib>
#include <cstring>

#include "sim/checkpoint.hh"

namespace SimpleSSD {

Bitset::Bitset() : pointer(nullptr), dataSize(0), allocSize(0), loopSize(0) {}

Bitset::Bitset(uint64_t size) : Bitset() {
  if (size > 0) {
    dataSize = size;
    allocSize = DIVCEIL(dataSize, 8);
    loopSize = getLoopCount();

    if (allocSize > sizeof(buffer)) {
      pointer = (uint8_t *)calloc(allocSize, 1);
    }
  }
}

Bitset::Bitset(Bitset &&rhs) noexcept : Bitset() {
  *this = std::move(rhs);
}

Bitset::~Bitset() {
  if (allocSize > sizeof(buffer)) {
    free(pointer);
  }

  pointer = nullptr;
  dataSize = 0;
  allocSize = 0;
  loopSize = 0;
}

bool Bitset::test(uint64_t idx) noexcept {
  return const_cast<const Bitset *>(this)->test(idx);
}

bool Bitset::test(uint64_t idx) const noexcept {
  auto data = getBuffer();

  return data[idx / 8] & (0x01 << (idx % 8));
}

bool Bitset::all() noexcept {
  return const_cast<const Bitset *>(this)->all();
}

bool Bitset::all() const noexcept {
  return count() == dataSize;
}

bool Bitset::any() noexcept {
  return const_cast<const Bitset *>(this)->any();
}

bool Bitset::any() const noexcept {
  return !none();
}

bool Bitset::none() noexcept {
  return const_cast<const Bitset *>(this)->none();
}

bool Bitset::none() const noexcept {
  return count() == 0;
}

uint64_t Bitset::clz() noexcept {
  return const_cast<const Bitset *>(this)->clz();
}

uint64_t Bitset::clz() const noexcept {
  uint64_t ret = 0;

  if (allocSize > sizeof(buffer)) {
    uint64_t i = allocSize - 1;

    for (; i > loopSize; i--) {
      if (pointer[i] != 0) {
        break;
      }

      ret++;
    }

    ret = (ret << 3) - (allocSize * 8 - dataSize);

    if (i == loopSize && pointer[loopSize] == 0) {
      uint64_t ret2 = 0;

      i -= 8;

      for (; i > 0; i -= 8) {
        if (*(uint64_t *)(pointer + i) != 0) {
          break;
        }

        ret2++;
      }

      if (UNLIKELY(i == 0 && *(uint64_t *)(pointer) == 0)) {
        return dataSize;
      }

      return ret + (ret2 << 6) + clz64(*(uint64_t *)(pointer + i));
    }

    return ret;
  }
  else {
    // Safe to case dataSize to int because dataSize <= 8 * sizeof(uint8_t *)
    uint64_t &value = *(uint64_t *)buffer;

    if (UNLIKELY(value == 0)) {
      return dataSize;
    }

    return clz64(value) - (64 - (int)dataSize);
  }
}

uint64_t Bitset::ctz() noexcept {
  return const_cast<const Bitset *>(this)->ctz();
}

uint64_t Bitset::ctz() const noexcept {
  uint64_t ret = 0;

  if (allocSize > sizeof(buffer)) {
    uint64_t i = 0;

    for (; i < loopSize; i += 8) {
      if (*(uint64_t *)(pointer + i) != 0) {
        break;
      }

      ret += 8;
    }

    for (; i < allocSize - 1; i++) {
      if (pointer[i] != 0) {
        break;
      }

      ret++;
    }

    if (UNLIKELY(i == allocSize && pointer[allocSize - 1] == 0)) {
      return dataSize;
    }

    return (ret << 3) + ctz8(pointer[allocSize - 1]);
  }
  else {
    uint64_t &value = *(uint64_t *)buffer;

    if (UNLIKELY(value == 0)) {
      return dataSize;
    }

    return ctz64(value);
  }
}

uint64_t Bitset::count() noexcept {
  return const_cast<const Bitset *>(this)->count();
}

uint64_t Bitset::count() const noexcept {
  if (allocSize > sizeof(buffer)) {
    uint64_t count = 0;

    for (uint64_t i = 0; i < loopSize; i += 8) {
      count += popcount64(*(uint64_t *)(pointer + i));
    }

    for (uint64_t i = loopSize; i < allocSize; i++) {
      count += popcount8(pointer[i]);
    }

    return count;
  }
  else {
    return popcount64(*(uint64_t *)buffer);
  }
}

uint64_t Bitset::size() noexcept {
  return const_cast<const Bitset *>(this)->size();
}

uint64_t Bitset::size() const noexcept {
  return dataSize;
}

void Bitset::set() noexcept {
  uint8_t mask = 0xFF >> (allocSize * 8 - dataSize);
  auto data = getBuffer();

  memset(data, 0xFF, allocSize);

  data[allocSize - 1] = mask;
}

void Bitset::set(uint64_t idx, bool value) noexcept {
  auto data = getBuffer();

  data[idx / 8] &= ~(0x01 << (idx % 8));

  if (value) {
    data[idx / 8] = data[idx / 8] | (0x01 << (idx % 8));
  }
}

void Bitset::reset() noexcept {
  auto data = getBuffer();

  memset(data, 0, allocSize);
}

void Bitset::reset(uint64_t idx) noexcept {
  auto data = getBuffer();

  data[idx / 8] &= ~(0x01 << (idx % 8));
}

void Bitset::flip() noexcept {
  uint8_t mask = 0xFF >> (allocSize * 8 - dataSize);
  auto data = getBuffer();

  for (uint64_t i = 0; i < loopSize; i += 8) {
    uint64_t &value = *(uint64_t *)(data + i);

    value = ~value;
  }

  for (uint64_t i = loopSize; i < allocSize; i++) {
    data[i] = ~data[i];
  }

  data[allocSize - 1] &= mask;
}

void Bitset::flip(uint64_t idx) noexcept {
  auto data = getBuffer();

  data[idx / 8] = (~data[idx / 8] & (0x01 << (idx % 8))) |
                  (data[idx / 8] & ~(0x01 << (idx % 8)));
}

bool Bitset::operator[](uint64_t idx) noexcept {
  return const_cast<const Bitset *>(this)->test(idx);
}

bool Bitset::operator[](uint64_t idx) const noexcept {
  return test(idx);
}

Bitset &Bitset::operator&=(const Bitset &rhs) {
  if (UNLIKELY(dataSize != rhs.dataSize)) {
    std::cerr << "Size does not match" << std::endl;

    abort();
  }

  auto data = getBuffer();
  auto rdata = rhs.getBuffer();

  for (uint64_t i = 0; i < loopSize; i++) {
    *(uint64_t *)(data + i) &= *(uint64_t *)(rdata + i);
  }

  for (uint64_t i = loopSize; i < allocSize; i++) {
    data[i] &= rdata[i];
  }

  return *this;
}

Bitset &Bitset::operator|=(const Bitset &rhs) {
  if (UNLIKELY(dataSize != rhs.dataSize)) {
    std::cerr << "Size does not match" << std::endl;

    abort();
  }

  auto data = getBuffer();
  auto rdata = rhs.getBuffer();

  for (uint64_t i = 0; i < loopSize; i++) {
    *(uint64_t *)(data + i) |= *(uint64_t *)(rdata + i);
  }

  for (uint64_t i = loopSize; i < allocSize; i++) {
    data[i] |= rdata[i];
  }

  return *this;
}

Bitset &Bitset::operator^=(const Bitset &rhs) {
  if (UNLIKELY(dataSize != rhs.dataSize)) {
    std::cerr << "Size does not match" << std::endl;

    abort();
  }

  auto data = getBuffer();
  auto rdata = rhs.getBuffer();

  for (uint64_t i = 0; i < loopSize; i++) {
    *(uint64_t *)(data + i) ^= *(uint64_t *)(rdata + i);
  }

  for (uint64_t i = loopSize; i < allocSize; i++) {
    data[i] ^= rdata[i];
  }

  return *this;
}

Bitset &Bitset::operator=(Bitset &&rhs) {
  if (this != &rhs) {
    // We need to copy when using size optimization
    if (rhs.allocSize <= sizeof(rhs.buffer)) {
      if (allocSize > sizeof(buffer)) {
        free(this->pointer);
      }
      else {
        memcpy(this->buffer, rhs.buffer, sizeof(buffer));
      }
    }
    else {
      if (allocSize > sizeof(buffer)) {
        free(this->pointer);
      }

      this->pointer = std::exchange(rhs.pointer, nullptr);
    }

    this->dataSize = std::exchange(rhs.dataSize, 0);
    this->allocSize = std::exchange(rhs.allocSize, 0);
    this->loopSize = std::exchange(rhs.loopSize, 0);
  }

  return *this;
}

void Bitset::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, dataSize);
  BACKUP_SCALAR(out, allocSize);
  BACKUP_SCALAR(out, loopSize);

  if (allocSize > 0) {
    if (allocSize > sizeof(buffer)) {
      BACKUP_BLOB(out, pointer, allocSize);
    }
    else {
      BACKUP_BLOB(out, buffer, sizeof(buffer));
    }
  }
}

void Bitset::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, dataSize);

  // Free or allocate memory if necessary
  *this = Bitset(dataSize);

  RESTORE_SCALAR(in, allocSize);
  RESTORE_SCALAR(in, loopSize);

  if (allocSize > 0) {
    if (allocSize > sizeof(buffer)) {
      RESTORE_BLOB(in, pointer, allocSize);
    }
    else {
      RESTORE_BLOB(in, buffer, sizeof(buffer));
    }
  }
}

}  // namespace SimpleSSD
