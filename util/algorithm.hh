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

#pragma once

#ifndef __UTIL_ALGORITHM__
#define __UTIL_ALGORITHM__

#include <cinttypes>
#include <climits>
#include <cmath>

#ifndef MIN
#define MIN(x, y) ((x) > (y) ? (y) : (x))
#endif

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#define DIVCEIL(x, y) (((x)-1) / (y) + 1)

namespace SimpleSSD {

template <typename T>
uint8_t popcount(T v) {
  v = v - ((v >> 1) & (T) ~(T)0 / 3);
  v = (v & (T) ~(T)0 / 15 * 3) + ((v >> 2) & (T) ~(T)0 / 15 * 3);
  v = (v + (v >> 4)) & (T) ~(T)0 / 255 * 15;
  v = (T)(v * ((T) ~(T)0 / 255)) >> (sizeof(T) - 1) * CHAR_BIT;

  return (uint8_t)v;
}

inline uint64_t generateMask(uint32_t val, uint32_t &count) {
  uint64_t mask = (uint64_t)-1;
  uint32_t tmp = 0;

  if (val > 0) {
    int shift = __builtin_clzl(val);

    if (shift + __builtin_ffsl(val) == 64) {
      shift++;
    }

    tmp = 64 - shift;
    mask = (mask << tmp);
  }

  mask = (~mask) << count;
  count += tmp;

  return mask;
}

inline uint16_t bswap16(uint16_t val) {
  return __builtin_bswap16(val);
}

}  // namespace SimpleSSD

#endif
