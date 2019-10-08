// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NVME_COMMAND_COMPARE_HH__
#define __SIMPLESSD_HIL_NVME_COMMAND_COMPARE_HH__

#include "hil/nvme/command/abstract_command.hh"

namespace SimpleSSD::HIL::NVMe {

class Compare : public Command {
 private:
  Event dmaInitEvent;
  Event dmaCompleteEvent;
  Event readNVMDoneEvent;

  uint8_t complete;

  uint64_t size;
  uint8_t *bufferHost;
  uint8_t *bufferNVM;

  uint64_t slpn;
  uint64_t nlp;
  uint32_t skipFront;
  uint32_t skipEnd;

  uint64_t _slba;
  uint16_t _nlb;

  uint64_t beginAt;

  void dmaInitDone();
  void dmaComplete();
  void readNVMDone();
  void compare();

 public:
  Compare(ObjectData &, Subsystem *, ControllerData *);
  ~Compare();

  void setRequest(SQContext *) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
