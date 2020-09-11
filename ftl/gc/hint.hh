// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_GC_HINT_HH__
#define __SIMPLESSD_FTL_GC_HINT_HH__

#include <cinttypes>

namespace SimpleSSD::FTL::GC {

//! GC hints for idle time detection
struct HintContext {
  uint64_t pendingRequests;   // # of requests which is not dispatched
  uint64_t handlingRequests;  // # of request dispatched but not completed

  uint64_t pendingReadBytes;   // Total read DMA bytes should be performed
  uint64_t pendingWriteBytes;  // Total write DMA bytes should be performed
  uint64_t handlingReadBytes;
  uint64_t handlingWriteBytes;

  uint64_t allocatableBytes;   // Total bytes that ICL can handle
  uint64_t evictPendingBytes;  // Total bytes that evicted in near-future

  HintContext()
      : pendingRequests(0),
        handlingRequests(0),
        pendingReadBytes(0),
        pendingWriteBytes(0),
        handlingReadBytes(0),
        handlingWriteBytes(0),
        allocatableBytes(0),
        evictPendingBytes(0) {}
};

}  // namespace SimpleSSD::FTL::GC

#endif
