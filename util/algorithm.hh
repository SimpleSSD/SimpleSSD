// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __UTIL_ALGORITHM__
#define __UTIL_ALGORITHM__

#include <cinttypes>
#include <climits>
#include <cmath>

#ifdef _MSC_VER

#include <intrin.h>

#include <cstdlib>

#define __builtin_bswap16 _byteswap_ushort
#define __builtin_bswap32 _byteswap_ulong
#define __builtin_bswap64 _byteswap_uint64

inline uint32_t __builtin_clzl(uint32_t val) {
  unsigned long leadingZero = 0;

  if (_BitScanReverse(&leadingZero, val)) {
    return 31 - leadingZero;
  }

  return 32;
}

inline uint32_t __builtin_ffsl(uint32_t val) {
  unsigned long trailingZero = 0;

  if (_BitScanForward(&trailingZero, val)) {
    return trailingZero + 1;
  }

  return 0;
}

inline uint64_t __builtin_ffsll(uint64_t val) {
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

#endif

#ifndef MIN
#define MIN(x, y) ((x) > (y) ? (y) : (x))
#endif

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#define DIVCEIL(x, y) (((x)-1) / (y) + 1)

namespace SimpleSSD {

template <typename T, std::enable_if_t<std::is_integral_v<T>> = 0>
uint8_t popcount(T v) {
  v = v - ((v >> 1) & (T) ~(T)0 / 3);
  v = (v & (T) ~(T)0 / 15 * 3) + ((v >> 2) & (T) ~(T)0 / 15 * 3);
  v = (v + (v >> 4)) & (T) ~(T)0 / 255 * 15;
  v = (T)(v * ((T) ~(T)0 / 255)) >> (sizeof(T) - 1) * CHAR_BIT;

  return (uint8_t)v;
}

inline uint8_t fastlog2(uint64_t val) {
  return (uint8_t)__builtin_ffsll(val) - 1;
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
