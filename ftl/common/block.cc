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

#include "log/trace.hh"

namespace SimpleSSD {

namespace FTL {

Block::Block(uint32_t count)
    : pageCount(count), nextWritePageIndex(0), lastAccessed(0), eraseCount(0) {
  validBits.resize(pageCount);
  erasedBits.resize(pageCount);
  lpns.resize(pageCount);

  erase();
  eraseCount = 0;
}

Block::~Block() {}

void Block::increasePageIndex(uint32_t &pageIndex) {
  pageIndex++;

  if (pageIndex == pageCount) {
    pageIndex = 0;
  }
}

uint64_t Block::getLastAccessedTime() {
  return lastAccessed;
}

uint32_t Block::getEraseCount() {
  return eraseCount;
}

uint32_t Block::getValidPageCount() {
  uint32_t ret = 0;

  for (auto iter : validBits) {
    if (iter) {
      ret++;
    }
  }

  return ret;
}

uint32_t Block::getNextWritePageIndex() {
  return nextWritePageIndex;
}

bool Block::read(uint32_t pageIndex, uint64_t *pLPN, uint64_t tick) {
  bool valid = validBits.at(pageIndex);

  if (valid) {
    lastAccessed = tick;

    if (pLPN) {
      *pLPN = lpns.at(pageIndex);
    }
  }

  return valid;
}

bool Block::write(uint32_t pageIndex, uint64_t lpn, uint64_t tick) {
  bool valid = erasedBits.at(pageIndex);

  if (valid) {
    if (pageIndex != nextWritePageIndex) {
      Logger::panic("Write to block should sequential");
    }

    lastAccessed = tick;
    erasedBits.at(pageIndex) = false;
    validBits.at(pageIndex) = true;
    lpns.at(pageIndex) = lpn;

    increasePageIndex(nextWritePageIndex);
  }
  else {
    Logger::panic("Write to dirty page");
  }

  return valid;
}

void Block::erase() {
  for (auto iter : validBits) {
    iter = false;
  }
  for (auto iter : erasedBits) {
    iter = true;
  }

  eraseCount++;
  nextWritePageIndex = 0;
}

void Block::invalidate(uint32_t pageIndex) {
  validBits.at(pageIndex) = false;
}

}  // namespace FTL

}  // namespace SimpleSSD
