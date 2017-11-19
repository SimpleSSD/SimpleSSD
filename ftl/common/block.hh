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

#ifndef __FTL_COMMON_BLOCK__
#define __FTL_COMMON_BLOCK__

#include <cinttypes>
#include <vector>

namespace SimpleSSD {

namespace FTL {

class Block {
 private:
  const uint32_t pageCount;
  uint32_t nextWritePageIndex;

  std::vector<bool> validBits;
  std::vector<bool> erasedBits;
  std::vector<uint64_t> lpns;

  uint64_t lastAccessed;
  uint32_t eraseCount;

  void increasePageIndex(uint32_t &);

 public:
  Block(uint32_t);
  ~Block();

  uint64_t getLastAccessedTime();
  uint32_t getEraseCount();
  uint32_t getValidPageCount();
  uint32_t getNextWritePageIndex();
  bool read(uint32_t, uint64_t *, uint64_t);
  bool write(uint32_t, uint64_t, uint64_t);
  void erase();
  void invalidate(uint32_t);
};

}  // namespace FTL

}  // namespace SimpleSSD

#endif
