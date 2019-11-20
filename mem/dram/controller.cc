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
  switch ((Memory::Config::Model)readConfigUint(
      Section::Memory, Memory::Config::Key::DRAMModel)) {
    case Memory::Config::Model::Ideal:
      pDRAM = new Memory::DRAM::Ideal(object);
      break;
    case Memory::Config::Model::LPDDR4:
      // object.dram = new Memory::DRAM::TimingDRAM(object);
      break;
    default:
      std::cerr << "Invalid DRAM model selected." << std::endl;

      abort();
  }
}

DRAMController::~DRAMController() {
  delete pDRAM;
}

void DRAMController::getStatList(std::vector<Stat> &list,
                                 std::string prefix) noexcept {
  pDRAM->getStatList(list, prefix + "dram.");
}

void DRAMController::getStatValues(std::vector<double> &values) noexcept {
  pDRAM->getStatValues(values);
}

void DRAMController::resetStatValues() noexcept {
  pDRAM->resetStatValues();
}

void DRAMController::createCheckpoint(std::ostream &out) const noexcept {
  AbstractRAM::createCheckpoint(out);

  pDRAM->createCheckpoint(out);
}

void DRAMController::restoreCheckpoint(std::istream &in) noexcept {
  AbstractRAM::restoreCheckpoint(in);

  pDRAM->restoreCheckpoint(in);
}

}  // namespace SimpleSSD::Memory::DRAM
