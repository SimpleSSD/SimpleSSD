// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NVME_COMMAND_IDENTIFY_HH__
#define __SIMPLESSD_HIL_NVME_COMMAND_IDENTIFY_HH__

#include "hil/nvme/command/abstract_command.hh"
#include "hil/nvme/def.hh"
#include "sim/abstract_subsystem.hh"

namespace SimpleSSD::HIL::NVMe {

class Identify : public Command {
 private:
  Event dmaInitEvent;
  Event dmaCompleteEvent;

  void makeNamespaceStructure(BufferCommandData *, uint32_t, bool = false);
  void makeNamespaceList(BufferCommandData *, uint32_t, bool = false);
  void makeControllerStructure(BufferCommandData *);
  void makeControllerList(BufferCommandData *, ControllerID,
                          uint32_t = NSID_ALL);

  void dmaInitDone(uint64_t);
  void dmaComplete(uint64_t);

 public:
  Identify(ObjectData &, Subsystem *);

  void setRequest(ControllerData *, SQContext *) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
