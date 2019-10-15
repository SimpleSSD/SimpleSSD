// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "fil/fil.hh"

namespace SimpleSSD::FIL {

FIL::FIL(ObjectData &o) : Object(o) {
  auto channel = readConfigUint(Section::FlashInterface, Config::Key::Channel);
  auto way = readConfigUint(Section::FlashInterface, Config::Key::Way);
  auto param = object.config->getNANDStructure();

  debugprint(Log::DebugID::FIL,
             "Channel |   Way   |   Die   |  Plane  |  Block  |   Page  ");
  debugprint(Log::DebugID::FIL, "%7u | %7u | %7u | %7u | %7u | %7u", channel,
             way, param->die, param->plane, param->block, param->page);
  debugprint(Log::DebugID::FIL, "Page size: %u + %u", param->pageSize,
             param->spareSize);

  switch ((Config::NVMType)readConfigUint(Section::FlashInterface,
                                          Config::Key::Model)) {
    case Config::NVMType::PAL:
      break;
    case Config::NVMType::GenericNAND:
      break;
  }
}

FIL::~FIL() {
  delete pFIL;
}

void FIL::getStatList(std::vector<Stat> &list, std::string prefix) {
  pFIL->getStatList(list, prefix + "fil.");
}

void FIL::getStatValues(std::vector<double> &values) {
  pFIL->getStatValues(values);
}

void FIL::resetStatValues() {
  pFIL->resetStatValues();
}

void FIL::createCheckpoint(std::ostream &out) const noexcept {
  pFIL->createCheckpoint(out);
}
void FIL::restoreCheckpoint(std::istream &in) noexcept {
  pFIL->restoreCheckpoint(in);
}

}  // namespace SimpleSSD::FIL