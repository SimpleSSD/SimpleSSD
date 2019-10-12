// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NVME_COMMAND_WRITE_HH__
#define __SIMPLESSD_HIL_NVME_COMMAND_WRITE_HH__

#include "hil/nvme/command/abstract_command.hh"

namespace SimpleSSD::HIL::NVMe {

class Write : public Command {
 private:
  Event dmaInitEvent;
  Event writeDoneEvent;
  Event dmaCompleteEvent;

  void dmaInitDone(uint64_t);
  void dmaComplete(uint64_t);
  void writeDone(uint64_t);

 public:
  Write(ObjectData &, Subsystem *);

  void setRequest(ControllerData *, SQContext *) override;
  void completeRequest(CommandTag) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
