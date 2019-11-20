// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/dram/controller.hh"

#include "mem/dram/ideal.hh"

namespace SimpleSSD::Memory::DRAM {

DRAMController::DRAMController(ObjectData &o) : AbstractRAM(o) {
  ctrl = object.config->getDRAMController();

  totalChannel = object.config->getDRAM()->channel;

  pDRAM.resize(totalChannel);

  // Create DRAM Objects
  for (auto &dram : pDRAM) {
    switch ((Memory::Config::Model)readConfigUint(
        Section::Memory, Memory::Config::Key::DRAMModel)) {
      case Memory::Config::Model::Ideal:
        dram = new Memory::DRAM::Ideal(object);
        break;
      case Memory::Config::Model::LPDDR4:
        // dram = new Memory::DRAM::TimingDRAM(object);
        break;
      default:
        std::cerr << "Invalid DRAM model selected." << std::endl;

        abort();
    }
  }
}

DRAMController::~DRAMController() {
  for (auto &dram : pDRAM) {
    delete dram;
  }
}

void DRAMController::getStatList(std::vector<Stat> &list,
                                 std::string prefix) noexcept {
  for (uint8_t i = 0; i < totalChannel; i++) {
    pDRAM[i]->getStatList(list, prefix + "channel" + std::to_string(i) + ".");
  }
}

void DRAMController::getStatValues(std::vector<double> &values) noexcept {
  for (auto &dram : pDRAM) {
    dram->getStatValues(values);
  }
}

void DRAMController::resetStatValues() noexcept {
  for (auto &dram : pDRAM) {
    dram->resetStatValues();
  }
}

void DRAMController::createCheckpoint(std::ostream &out) const noexcept {
  AbstractRAM::createCheckpoint(out);

  BACKUP_SCALAR(out, totalChannel);

  for (auto &dram : pDRAM) {
    dram->createCheckpoint(out);
  }
}

void DRAMController::restoreCheckpoint(std::istream &in) noexcept {
  AbstractRAM::restoreCheckpoint(in);

  uint8_t t8;

  RESTORE_SCALAR(in, t8);

  panic_if(t8 != totalChannel, "DRAM channel mismatch while restore.");

  for (auto &dram : pDRAM) {
    dram->restoreCheckpoint(in);
  }
}

}  // namespace SimpleSSD::Memory::DRAM
