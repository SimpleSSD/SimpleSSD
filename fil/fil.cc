// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "fil/fil.hh"

#include "fil/nvm/pal/pal_wrapper.hh"
#include "fil/scheduler/noop.hh"

namespace SimpleSSD::FIL {

FIL::FIL(ObjectData &o, CommandManager *m) : Object(o), commandManager(m) {
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
      pNVM = new NVM::PALOLD(object, commandManager);

      break;
    // case Config::NVMType::GenericNAND:
    default:
      panic("Unexpected FIL model.");

      break;
  }

  pScheduler = new Scheduler::Noop(object, commandManager, pNVM);
}

FIL::~FIL() {
  delete pScheduler;
  delete pNVM;
}

void FIL::submit(uint64_t tag) {
  pScheduler->enqueue(tag);
}

void FIL::writeSpare(PPN ppn, std::vector<uint8_t> &spare) {
  pNVM->writeSpare(ppn, spare);
}

void FIL::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  pNVM->getStatList(list, prefix + "fil.nvm.");
  pScheduler->getStatList(list, prefix + "fil.scheduler.");
}

void FIL::getStatValues(std::vector<double> &values) noexcept {
  pNVM->getStatValues(values);
  pScheduler->getStatValues(values);
}

void FIL::resetStatValues() noexcept {
  pNVM->resetStatValues();
  pScheduler->resetStatValues();
}

void FIL::createCheckpoint(std::ostream &out) const noexcept {
  pNVM->createCheckpoint(out);
  pScheduler->createCheckpoint(out);
}
void FIL::restoreCheckpoint(std::istream &in) noexcept {
  pNVM->restoreCheckpoint(in);
  pScheduler->restoreCheckpoint(in);
}

}  // namespace SimpleSSD::FIL
