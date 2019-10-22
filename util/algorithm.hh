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

#define bswap16(x) _byteswap_ushort((uint16_t)(x))
#define bswap32(x) _byteswap_ulong((uint32_t)(x))
#define bswap64(x) _byteswap_uint64((uint64_t)(x))
#define popcount8(x) __popcnt16((uint16_t)(x))
#define popcount16(x) __popcnt16((uint16_t)(x))
#define popcount32(x) __popcnt((uint32_t)(x))
#define popcount64(x) __popcnt64((uint64_t)(x))

#define clz8(x) clz32((uint32_t)0xFFFFFF00 | (uint32_t)(x))
#define clz16(x) clz32((uint32_t)0xFFFF0000 | (uint32_t)(x))

inline uint32_t clz32(uint32_t val) {
  unsigned long leadingZero = 0;

  if (_BitScanReverse(&leadingZero, val)) {
    return 31 - leadingZero;
  }

  return 32;
}

inline uint32_t clz64(uint32_t val) {
  unsigned long leadingZero = 0;

  if (_BitScanReverse64(&leadingZero, val)) {
    return 63 - leadingZero;
  }

  return 64;
}

#define ctz8(x) ctz32((uint32_t)(x))
#define ctz16(x) ctz32((uint32_t)(x))

inline uint32_t ctz32(uint32_t val) {
  unsigned long trailingZero = 0;

  if (_BitScanForward(&trailingZero, val)) {
    return trailingZero;
  }

  return 64;
}

inline uint64_t ctz64(uint64_t val) {
  unsigned long trailingZero = 0;

  if (_BitScanForward64(&trailingZero, val)) {
    return trailingZero;
  }

  return 64;
}

#define LIKELY
#define UNLIKELY

#else

#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#define popcount8(x) __builtin_popcount((uint32_t)(x))
#define popcount16(x) __builtin_popcount((uint32_t)(x))
#define popcount32(x) __builtin_popcount((uint32_t)(x))
#define clz8(x) __builtin_clz((uint32_t)0xFFFFFF00 | (uint32_t)(x))
#define clz16(x) __builtin_clz((uint32_t)0xFFFF0000 | (uint32_t)(x))
#define clz32(x) __builtin_clz((uint32_t)(x))
#define ctz8(x) __builtin_ctz((uint32_t)(x))
#define ctz16(x) __builtin_ctz((uint32_t)(x))
#define ctz32(x) __builtin_ctz((uint32_t)(x))

#if __WORDSIZE == 64
#define popcount64(x) __builtin_popcountl((uint64_t)(x))
#define clz64(x) __builtin_clzl((uint64_t)(x))
#define ctz64(x) __builtin_ctzl((uint64_t)(x))
#else
#define popcount64(x) __builtin_popcountll((uint64_t)(x))
#define clz64(x) __builtin_clzll((uint64_t)(x))
#define ctz64(x) __builtin_ctzll((uint64_t)(x))
#endif

#endif

#ifndef MIN
#define MIN(x, y) ((x) > (y) ? (y) : (x))
#endif

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#define DIVCEIL(x, y) (((x)-1) / (y) + 1)

#define MAKE64(h32, l32) (((uint64_t)h32 << 32) | l32)
#define MAKE32(h16, l16) (((uint32_t)h16 << 16) | l16)
#define HIGH32(v64) ((uint32_t)(v64 >> 32))
#define HIGH16(v32) ((uint16_t)(v32 >> 16))
#define LOW32(v64) ((uint32_t)v64)
#define LOW16(v32) ((uint16_t)v32)

#endif
