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

#ifndef __FTL_LATENCY__
#define __FTL_LATENCY__

#include <cinttypes>
#include <queue>
#include <vector>

namespace SimpleSSD {

namespace FTL {

class Latency {
 private:
  std::priority_queue<uint64_t, std::vector<uint64_t>, std::greater<uint64_t>>
      lastFTLRequestAt;
  uint64_t latency;

 public:
  Latency(uint64_t, uint64_t);
  ~Latency();

  void access(uint32_t size, uint64_t &tick);
};

}  // namespace FTL

}  // namespace SimpleSSD

#endif
