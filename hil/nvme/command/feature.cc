// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/feature.hh"

namespace SimpleSSD::HIL::NVMe {

Feature::Feature(ObjectData &o) : Object(o) {
  pm.data = 0;
  er.data = 0;
  wan = 0;
  aec.data = 0;
  aec.nan = 1;  // Send namespace notification

  // Fill dummy values
  for (uint8_t i = 0; i < 10; i++) {
    underThresholdList[i] = 288;  // 288K = 15C
    overThresholdList[i] = 363;   // 363K = 90C
  }
}

void Feature::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Feature::getStatValues(std::vector<double> &) noexcept {}

void Feature::resetStatValues() noexcept {}

void Feature::createCheckpoint(std::ostream &out) noexcept {
  BACKUP_SCALAR(out, pm.data);
  BACKUP_SCALAR(out, er.data);
  BACKUP_SCALAR(out, noq.data);
  BACKUP_SCALAR(out, wan);
  BACKUP_SCALAR(out, aec.data);
  BACKUP_BLOB(out, overThresholdList.data(), 18);
}

void Feature::restoreCheckpoint(std::istream &in) noexcept {

  RESTORE_SCALAR(in, pm.data);
  RESTORE_SCALAR(in, er.data);
  RESTORE_SCALAR(in, noq.data);
  RESTORE_SCALAR(in, wan);
  RESTORE_SCALAR(in, aec.data);
  RESTORE_BLOB(in, overThresholdList.data(), 18)
}

}  // namespace SimpleSSD::HIL::NVMe
