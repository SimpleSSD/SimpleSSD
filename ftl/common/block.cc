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

#include "ftl/common/block.hh"

#include <algorithm>

#include "log/trace.hh"

namespace SimpleSSD {

namespace FTL {

Block::Block(uint32_t count, uint32_t ioUnit)
    : pageCount(count),
      ioUnitInPage(ioUnit),
      nextWritePageIndex(std::vector<uint32_t>(ioUnitInPage, 0)),
      validBits(
          std::vector<DynamicBitset>(pageCount, DynamicBitset(ioUnitInPage))),
      erasedBits(
          std::vector<DynamicBitset>(pageCount, DynamicBitset(ioUnitInPage))),
      lpns(std::vector<std::vector<uint64_t>>(
          pageCount, std::vector<uint64_t>(ioUnitInPage, 0))),
      lastAccessed(0),
      eraseCount(0) {
  erase();
  eraseCount = 0;
}

Block::~Block() {}

uint64_t Block::getLastAccessedTime() {
  return lastAccessed;
}

uint32_t Block::getEraseCount() {
  return eraseCount;
}

uint32_t Block::getValidPageCount() {
  uint32_t ret = 0;

  for (auto &iter : validBits) {
    if (iter.any()) {
      ret++;
    }
  }

  return ret;
}

uint32_t Block::getDirtyPageCount() {
  uint32_t ret = 0;

  for (uint32_t i = 0; i < pageCount; i++) {
    // Dirty: Valid(false), Erased(false)
    if ((~(validBits.at(i) | erasedBits.at(i))).any()) {
      ret++;
    }
  }

  return ret;
}

uint32_t Block::getNextWritePageIndex() {
  return *std::max_element(nextWritePageIndex.begin(),
                           nextWritePageIndex.end());
}

uint32_t Block::getNextWritePageIndex(uint32_t idx) {
  return nextWritePageIndex[idx];
}

bool Block::getPageInfo(uint32_t pageIndex, std::vector<uint64_t> &lpn,
                        DynamicBitset &map) {
  map = validBits.at(pageIndex);
  lpn = lpns.at(pageIndex);

  return map.any();
}

bool Block::read(uint32_t pageIndex, uint32_t idx, uint64_t tick) {
  auto valid = validBits.at(pageIndex);
  bool read = valid.test(idx);

  if (read) {
    lastAccessed = tick;
  }

  return read;
}

bool Block::write(uint32_t pageIndex, uint64_t lpn, uint32_t idx,
                  uint64_t tick) {
  auto valid = erasedBits.at(pageIndex);
  bool write = valid.test(idx);

  if (write) {
    if (pageIndex < nextWritePageIndex[idx]) {
      Logger::panic("Write to block should sequential");
    }

    lastAccessed = tick;
    erasedBits.at(pageIndex).reset(idx);
    validBits.at(pageIndex).set(idx);

    nextWritePageIndex[idx] = pageIndex + 1;
    lpns.at(pageIndex).at(idx) = lpn;
  }
  else {
    Logger::panic("Write to non erased page");
  }

  return write;
}

void Block::erase() {
  for (auto &iter : validBits) {
    iter.reset();
  }
  for (auto &iter : erasedBits) {
    iter.set();
  }
  for (auto &iter : nextWritePageIndex) {
    iter = 0;
  }

  eraseCount++;
}

void Block::invalidate(uint32_t pageIndex, uint32_t idx) {
  validBits.at(pageIndex).reset(idx);
}

}  // namespace FTL

}  // namespace SimpleSSD
