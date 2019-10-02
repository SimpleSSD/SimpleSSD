// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __HIL_COMMON_DMA_ENGINE_HH__
#define __HIL_COMMON_DMA_ENGINE_HH__

#include "sim/interface.hh"
#include "sim/object.hh"

namespace SimpleSSD::HIL {

class DMAEngine : public Interface, public Object {
 protected:
  Interface *pInterface;

  uint64_t callCounter;  //!< PRP/SGL/PRDT sub DMA request counter

  Event dmaHandler;

  static void dmaDone(uint64_t, void *);

 public:
  DMAEngine(ObjectData &&, Interface *);
  DMAEngine(const DMAEngine &) = delete;
  DMAEngine(DMAEngine &&) noexcept = default;
  virtual ~DMAEngine();

  DMAEngine &operator=(const DMAEngine &) = delete;
  DMAEngine &operator=(DMAEngine &&) noexcept = default;
};

}  // namespace SimpleSSD::HIL

#endif
