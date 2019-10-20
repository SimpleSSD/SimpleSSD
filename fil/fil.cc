// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "fil/fil.hh"

#include "fil/nvm/pal/pal_wrapper.hh"

namespace SimpleSSD::FIL {

FIL::FIL(ObjectData &o, HIL::CommandManager *m) : Object(o), commandManager(m) {
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
      pFIL = new NVM::PALOLD(object, commandManager);

      break;
    // case Config::NVMType::GenericNAND:
    default:
      panic("Unexpected FIL model.");

      break;
  }
}

FIL::~FIL() {
  delete pFIL;
}

void FIL::submit(uint64_t tag) {
  auto &list = commandManager->getSubCommand(tag);

  for (auto &scmd : list) {
    pFIL->enqueue(scmd);
  }
}

void FIL::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  pFIL->getStatList(list, prefix + "fil.");
}

void FIL::getStatValues(std::vector<double> &values) noexcept {
  pFIL->getStatValues(values);
}

void FIL::resetStatValues() noexcept {
  pFIL->resetStatValues();
}

void FIL::createCheckpoint(std::ostream &out) const noexcept {
  pFIL->createCheckpoint(out);
}
void FIL::restoreCheckpoint(std::istream &in) noexcept {
  pFIL->restoreCheckpoint(in);
}

}  // namespace SimpleSSD::FIL
