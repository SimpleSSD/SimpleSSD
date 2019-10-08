// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_COMMON_DMA_ENGINE_HH__
#define __SIMPLESSD_HIL_COMMON_DMA_ENGINE_HH__

#include "sim/interface.hh"
#include "sim/object.hh"

namespace SimpleSSD::HIL {

/**
 * \brief DMA Engine abstract class
 *
 * DMA engine class for SSD controllers.
 */
class DMAEngine : public DMAInterface, public Object {
 protected:
  class DMAContext {
   public:
    int32_t counter;
    Event eid;

    DMAContext();
    DMAContext(Event);
  };

  DMAInterface *pInterface;

  Event dmaHandler;
  DMAContext dmaContext;

  void dmaDone(uint64_t);

 public:
  DMAEngine(ObjectData &, DMAInterface *);
  DMAEngine(const DMAEngine &) = delete;
  DMAEngine(DMAEngine &&) noexcept = default;
  virtual ~DMAEngine();

  DMAEngine &operator=(const DMAEngine &) = delete;
  DMAEngine &operator=(DMAEngine &&) noexcept = default;

  virtual void init(uint64_t, uint64_t, uint64_t, Event) = 0;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL

#endif
