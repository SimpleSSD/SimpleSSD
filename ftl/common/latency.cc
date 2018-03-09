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

#include "ftl/common/latency.hh"

namespace SimpleSSD {

namespace FTL {

Latency::Latency(uint64_t l, uint64_t queueSize) : latency(l) {
  for (uint64_t i = 0; i < queueSize; i++) {
    lastFTLRequestAt.push(0);
  }
}

Latency::~Latency() {}

void Latency::access(uint32_t size, uint64_t &tick) {
  if (tick > 0) {
    uint64_t smallest = lastFTLRequestAt.top();

    if (smallest <= tick) {
      smallest = tick + latency * size;
    }
    else {
      smallest += latency * size;
    }

    tick = smallest;

    lastFTLRequestAt.pop();
    lastFTLRequestAt.push(smallest);
  }
}

}  // namespace FTL

}  // namespace SimpleSSD
