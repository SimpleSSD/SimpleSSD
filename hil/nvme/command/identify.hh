// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __HIL_NVME_COMMAND_IDENTIFY_HH__
#define __HIL_NVME_COMMAND_IDENTIFY_HH__

#include "hil/nvme/command/abstract_command.hh"
#include "hil/nvme/def.hh"

namespace SimpleSSD::HIL::NVMe {

class Identify : public Command {
 private:
  Event dmaInitEvent;

  const uint64_t size = 4096;
  uint8_t *buffer;

  void makeNamespaceStructure(uint32_t, bool = false);
  void makeNamespaceList(uint32_t, bool = false);
  void makeControllerStructure();
  void makeControllerList(ControllerID, uint32_t = NSID_ALL);

  void dmaInitDone(uint64_t);

 public:
  Identify(ObjectData &, Subsystem *, ControllerData *);
  ~Identify();

  void setRequest(SQContext *, Event) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
