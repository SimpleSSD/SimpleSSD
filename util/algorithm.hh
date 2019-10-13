// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_UTIL_ALGORITHM_HH__
#define __SIMPLESSD_UTIL_ALGORITHM_HH__

#include <cinttypes>
#include <climits>
#include <cmath>

#ifdef _MSC_VER

#include <intrin.h>

#include <cstdlib>

#define bswap16 _byteswap_ushort
#define bswap32 _byteswap_ulong
#define bswap64 _byteswap_uint64
#define popcount16 __popcnt16
#define popcount32 __popcnt
#define popcount64 __popcnt64

inline uint32_t clz32(uint32_t val) {
  unsigned long leadingZero = 0;

  if (_BitScanReverse(&leadingZero, val)) {
    return 31 - leadingZero;
  }

  return 32;
}

inline uint32_t ffs32(uint32_t val) {
  unsigned long trailingZero = 0;

  if (_BitScanForward(&trailingZero, val)) {
    return trailingZero + 1;
  }

  return 0;
}

inline uint64_t ffs64(uint64_t val) {
  unsigned long trailingZero = 0;

  if (_BitScanForward64(&trailingZero, val)) {
    return trailingZero + 1;
  }

  return 0;
}

#define LIKELY
#define UNLIKELY

#else

#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#define popcount16 __builtin_popcount
#define popcount32 __builtin_popcount
#define clz16 __builtin_clz
#define clz32 __builtin_clz
#define ffs16 __builtin_ffs
#define ffs32 __builtin_ffs

#if __WORDSIZE == 64
#define popcount64 __builtin_popcountl
#define clz64 __builtin_clzl
#define ffs64 __builtin_ffsl
#else
#define popcount64 __builtin_popcountll
#define clz64 __builtin_clzll
#define ffs64 __builtin_ffsll
#endif

#endif

#ifndef MIN
#define MIN(x, y) ((x) > (y) ? (y) : (x))
#endif

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#define DIVCEIL(x, y) (((x)-1) / (y) + 1)

namespace SimpleSSD {

inline uint64_t generateMask(uint32_t val, uint32_t &count) {
  uint64_t mask = (uint64_t)-1;
  uint32_t tmp = 0;

  if (val > 0) {
    int shift = clz32(val);

    if (shift + ffs32(val) == 64) {
      shift++;
    }

    tmp = 64 - shift;
    mask = (mask << tmp);
  }

  mask = (~mask) << count;
  count += tmp;

  return mask;
}

}  // namespace SimpleSSD

#endif
