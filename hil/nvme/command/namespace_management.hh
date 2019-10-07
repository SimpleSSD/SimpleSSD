// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NVME_COMMAND_NAMESPACE_MANAGEMENT_HH__
#define __SIMPLESSD_HIL_NVME_COMMAND_NAMESPACE_MANAGEMENT_HH__

#include "hil/nvme/command/abstract_command.hh"

namespace SimpleSSD::HIL::NVMe {

class NamespaceManagement : public Command {
 private:
  Event dmaInitEvent;
  Event dmaCompleteEvent;

  const uint64_t size = 4096;
  uint8_t *buffer;

  void dmaInitDone();
  void dmaComplete();

 public:
  NamespaceManagement(ObjectData &, Subsystem *, ControllerData *);
  ~NamespaceManagement();

  void setRequest(SQContext *) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
