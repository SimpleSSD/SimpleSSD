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

/**
 * \brief NVMe Compare command
 *
 * Perform compare operation. To overlap PCIe DMA and DRAM access, it access in
 * sector granularity (512B ~ 4K).
 * If request has 16KB block size, we don't wait all 16KB is transfered through
 * PCIe bus. After one sector (e.g., 4K) has been transfered, DRAM access begin.
 *
 * Before overlapping:
 *   PCIe bus util. | [  4K  ][  4K  ][  4K  ][  4K  ]
 *   DRAM access    |                                 [4K][4K][4K][4K]
 *   Compare        |                                                 [Compare]
 *   DRAM access    |          [4K][4K][4K][4K]
 *   NAND access    | [  16K  ]
 *
 * After overlapping:
 *   PCIe bus util. | [  4K  ][  4K  ][  4K  ][  4K  ]
 *   DRAM access    |         [4K]    [4K]    [4K]    [4K]
 *   Compare        |                                         [Compare]
 *   DRAM access    |             [4K]    [4K]    [4K]    [4K]
 *   NAND access    | [  4K  ][  4K  ][  4K  ][  4K  ]
 */
namespace SimpleSSD::HIL::NVMe {

class Compare : public Command {
 private:
  Event eventDMAInitDone;
  void dmaInitDone(uint64_t);

  Event eventCompletion;
  void completion(uint64_t, uint64_t);

 public:
  Compare(ObjectData &, Subsystem *);

  void setRequest(ControllerData *, AbstractNamespace *, SQContext *) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
