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
    : AbstractBlockCopyJob(o, fo, fil), state(State::Idle) {
  auto mode = (Config::VictimSelectionMode)readConfigUint(
      Section::FlashTranslation, Config::Key::VictimSelectionPolicy);
  BlockAllocator::VictimSelectionID id =
      BlockAllocator::VictimSelectionID::Random;

  switch (mode) {
    case Config::VictimSelectionMode::Random:
      id = BlockAllocator::VictimSelectionID::Random;
      break;
    case Config::VictimSelectionMode::Greedy:
      id = BlockAllocator::VictimSelectionID::Greedy;
      break;
    case Config::VictimSelectionMode::CostBenefit:
      id = BlockAllocator::VictimSelectionID::CostBenefit;
      break;
    case Config::VictimSelectionMode::DChoice:
      id = BlockAllocator::VictimSelectionID::DChoice;
      break;
    default:
      panic("Unexpected victim block selection mode.");

      break;
  }

  method =
      BlockAllocator::VictimSelectionFactory::createVictiomSelectionAlgorithm(
          object, ftlobject.pAllocator, id);
}

AbstractGC::~AbstractGC() {}

void AbstractGC::initialize(bool) {}

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
    case TriggerType::ForegroundGCRequest:
      triggerForeground();

      break;
    default:
      break;
  }
}

void AbstractGC::createCheckpoint(std::ostream &out) const noexcept {
  AbstractBlockCopyJob::createCheckpoint(out);

  BACKUP_SCALAR(out, state);
}

void AbstractGC::restoreCheckpoint(std::istream &in) noexcept {
  AbstractBlockCopyJob::restoreCheckpoint(in);

  RESTORE_SCALAR(in, state);
}

}  // namespace SimpleSSD::FTL::GC
