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

#ifndef __UTIL_DEF__
#define __UTIL_DEF__

#include <cinttypes>
#include <string>
#include <vector>

#include "log/trace.hh"

#define MIN_LBA_SIZE 512

namespace SimpleSSD {

typedef struct _Stats {
  std::string name;
  std::string desc;
} Stats;

class StatObject {
 public:
  StatObject() {}
  virtual ~StatObject() {}

  virtual void getStats(std::vector<Stats> &) {}
  virtual void getStatValues(std::vector<uint64_t> &) {}
  virtual void resetStats() {}
};

typedef struct _LPNRange {
  uint64_t slpn;
  uint64_t nlp;

  _LPNRange();
  _LPNRange(uint64_t, uint64_t);
} LPNRange;

// TODO: use SIMD operation if possible
class DynamicBitset {
 private:
  std::vector<uint8_t> data;
  uint32_t dataSize;
  uint32_t allocSize;

  void boundCheck(uint32_t);

 public:
  DynamicBitset(uint32_t);

  bool test(uint32_t);
  bool all();
  bool any();
  bool none();
  uint32_t count();
  uint32_t size();
  void set();
  void set(uint32_t, bool = true);
  void reset();
  void reset(uint32_t);
  void flip();
  void flip(uint32_t);
  void print();

  bool operator[](uint32_t);
  DynamicBitset &operator&=(const DynamicBitset &);
  DynamicBitset &operator|=(const DynamicBitset &);
  DynamicBitset &operator^=(const DynamicBitset &);
  DynamicBitset operator~() const;

  friend DynamicBitset operator&(DynamicBitset lhs, const DynamicBitset &rhs) {
    return lhs &= rhs;
  }

  friend DynamicBitset operator|(DynamicBitset lhs, const DynamicBitset &rhs) {
    return lhs |= rhs;
  }

  friend DynamicBitset operator^(DynamicBitset lhs, const DynamicBitset &rhs) {
    return lhs ^= rhs;
  }

  friend bool operator==(const DynamicBitset &lhs, const DynamicBitset &rhs) {
    bool ret = true;

    if (lhs.dataSize != rhs.dataSize) {
      Logger::panic("Size does not match");
    }

    for (uint32_t i = 0; i < lhs.allocSize; i++) {
      if (lhs.data[i] != rhs.data[i]) {
        ret = false;

        break;
      }
    }

    return ret;
  }

  friend bool operator!=(const DynamicBitset &lhs, const DynamicBitset &rhs) {
    return !operator==(lhs, rhs);
  }
};

namespace ICL {

typedef struct _Request {
  uint64_t reqID;
  uint64_t reqSubID;
  uint64_t offset;
  uint64_t length;
  LPNRange range;

  _Request();
} Request;

}  // namespace ICL

namespace FTL {

typedef struct _Request {
  uint64_t reqID;  // ID of ICL::Request
  uint64_t reqSubID;
  uint64_t lpn;
  DynamicBitset ioFlag;

  _Request(uint32_t);
  _Request(uint32_t, ICL::Request &);
} Request;

}  // namespace FTL

namespace PAL {

typedef struct _Request {
  uint64_t reqID;  // ID of ICL::Request
  uint64_t reqSubID;
  uint32_t blockIndex;
  uint32_t pageIndex;
  DynamicBitset ioFlag;

  _Request(uint32_t);
  _Request(FTL::Request &);
} Request;

}  // namespace PAL

}  // namespace SimpleSSD

#endif
