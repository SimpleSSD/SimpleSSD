// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/gc/abstract_gc.hh"

#include "ftl/mapping/abstract_mapping.hh"

namespace SimpleSSD::FTL::GC {

AbstractGC::AbstractGC(ObjectData &o, FTLObjectData &fo, FIL::FIL *fil)
    : AbstractJob(o, fo), pFIL(fil), state(State::Idle), param(nullptr) {
  auto mode = (Config::VictimSelectionMode)readConfigUint(
      Section::FlashTranslation, Config::Key::VictimSelectionPolicy);

  switch (mode) {
    case Config::VictimSelectionMode::Random:
      method = BlockAllocator::getVictimSelectionAlgorithm(
          BlockAllocator::VictimSelectionID::Random);

      break;
    case Config::VictimSelectionMode::Greedy:
      method = BlockAllocator::getVictimSelectionAlgorithm(
          BlockAllocator::VictimSelectionID::Greedy);

      break;
    case Config::VictimSelectionMode::CostBenefit:
      method = BlockAllocator::getVictimSelectionAlgorithm(
          BlockAllocator::VictimSelectionID::CostBenefit);

      break;
    case Config::VictimSelectionMode::DChoice:
      method = BlockAllocator::getVictimSelectionAlgorithm(
          BlockAllocator::VictimSelectionID::DChoice);

      break;
    default:
      panic("Unexpected victim block selection mode.");

      break;
  }
}

AbstractGC::~AbstractGC() {}

void AbstractGC::initialize() {
  param = ftlobject.pMapping->getInfo();
}

bool AbstractGC::isRunning() {
  return state >= State::Foreground;
}

void AbstractGC::triggerByUser(TriggerType when, Request *req) {
  switch (when) {
    case TriggerType::ReadMapping:
    case TriggerType::WriteMapping:
      requestArrived(req);

      break;
    case TriggerType::WriteComplete:
      triggerForeground();

      break;
    default:
      break;
  }
}

void AbstractGC::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, state);
}

void AbstractGC::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, state);
}

}  // namespace SimpleSSD::FTL::GC
