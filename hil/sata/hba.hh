// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_SATA_HBA_HH__
#define __SIMPLESSD_HIL_SATA_HBA_HH__

#include "hil/common/interrupt_manager.hh"
#include "hil/sata/def.hh"
#include "sim/abstract_controller.hh"
#include "util/fifo.hh"

namespace SimpleSSD::HIL::SATA {

class HostBustAdapter : public AbstractController {
 protected:
 public:
  HostBustAdapter(ObjectData &, ControllerID, AbstractSubsystem *, Interface *);
  virtual ~HostBustAdapter();

  uint64_t read(uint64_t, uint32_t, uint8_t *) noexcept;
  uint64_t write(uint64_t, uint32_t, uint8_t *) noexcept;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::SATA

#endif
