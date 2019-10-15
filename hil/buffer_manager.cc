// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/buffer_manager.hh"

namespace SimpleSSD::HIL {

BufferManager::BufferManager(ObjectData &o) : Object(o) {}

void BufferManager::registerPointer(std::ostream &out, uint8_t *old,
                                    uint64_t size) const {
  BACKUP_SCALAR(out, old);
  BACKUP_SCALAR(out, size);
}

void BufferManager::updatePointer(std::istream &in, uint8_t *_new,
                                  uint64_t _size) {
  uint8_t *old;
  uint64_t size;

  RESTORE_SCALAR(in, old);
  RESTORE_SCALAR(in, size);

  panic_if(size != _size, "Size mismatch while restoring buffer list.");

  oldList.emplace_back(std::make_pair(old, size));
  newList.emplace_back(std::make_pair(_new, _size));
}

uint8_t *BufferManager::restorePointer(uint8_t *old) {
  if (old == nullptr) {
    return nullptr;
  }

  uint64_t size = oldList.size();

  // Find matching range including old pointer
  for (uint64_t i = 0; i < size; i++) {
    auto &iter = oldList.at(i);

    if (old >= iter.first && old < iter.first + iter.second) {
      // Matched range
      return newList.at(i).first + (old - iter.first);
    }
  }

  return nullptr;
}

void BufferManager::getStatList(std::vector<Stat> &, std::string) noexcept {}

void BufferManager::getStatValues(std::vector<double> &) noexcept {}

void BufferManager::resetStatValues() noexcept {}

void BufferManager::createCheckpoint(std::ostream &) const noexcept {}

void BufferManager::restoreCheckpoint(std::istream &) noexcept {}

}  // namespace SimpleSSD::HIL
