// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NVME_COMMAND_DATASET_MANAGEMENT_HH__
#define __SIMPLESSD_HIL_NVME_COMMAND_DATASET_MANAGEMENT_HH__

#include <list>

#include "hil/nvme/command/abstract_command.hh"

namespace SimpleSSD::HIL::NVMe {

class DatasetManagement : public Command {
 private:
  union Range {
    uint8_t data[16];
    struct {
      uint32_t ca;
      uint32_t nlb;
      uint64_t slba;
    };
  };

  Event dmaInitEvent;
  Event trimDoneEvent;
  Event dmaCompleteEvent;

  uint64_t size;
  uint8_t *buffer;

  uint64_t beginAt;

  std::list<std::pair<uint64_t, uint64_t>> trimList;

  void dmaInitDone();
  void dmaComplete();
  void trimDone();

 public:
  DatasetManagement(ObjectData &, Subsystem *, ControllerData *);
  ~DatasetManagement();

  void setRequest(SQContext *) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
