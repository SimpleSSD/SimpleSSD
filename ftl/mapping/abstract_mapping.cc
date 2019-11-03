// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/mapping/abstract_mapping.hh"

namespace SimpleSSD::FTL::Mapping {

AbstractMapping::AbstractMapping(ObjectData &o, CommandManager *c)
    : Object(o), commandManager(c), allocator(nullptr) {
  filparam = object.config->getNANDStructure();
  auto channel =
      readConfigUint(Section::FlashInterface, FIL::Config::Key::Channel);
  auto way = readConfigUint(Section::FlashInterface, FIL::Config::Key::Way);

  param.totalPhysicalBlocks =
      channel * way * filparam->die * filparam->plane * filparam->block;
  param.totalLogicalBlocks =
      (uint64_t)(param.totalPhysicalBlocks *
                 (1.f - readConfigFloat(Section::FlashTranslation,
                                        Config::Key::OverProvisioningRatio)));
  param.totalPhysicalPages = param.totalPhysicalBlocks * filparam->page;
  param.totalLogicalPages = param.totalLogicalBlocks * filparam->page;
  param.pageSize = filparam->pageSize;
  param.parallelism = channel * way * filparam->die * filparam->plane;

  for (uint8_t i = 0; i < 4; i++) {
    switch (filparam->pageAllocation[i]) {
      case FIL::PageAllocation::Channel:
        param.parallelismLevel[i] = (uint32_t)channel;
        break;
      case FIL::PageAllocation::Way:
        param.parallelismLevel[i] = (uint32_t)way;
        break;
      case FIL::PageAllocation::Die:
        param.parallelismLevel[i] = filparam->die;
        break;
      case FIL::PageAllocation::Plane:
        param.parallelismLevel[i] = filparam->plane;
        break;
      default:
        break;
    }
  }

  param.superpageLevel = (uint8_t)readConfigUint(
      Section::FlashTranslation, Config::Key::SuperpageAllocation);

  // Validate superpage level
  uint8_t mask = FIL::PageAllocation::None;
  param.superpage = 1;

  for (uint8_t i = 0; i < 4; i++) {
    if (param.superpageLevel & filparam->pageAllocation[i]) {
      mask |= filparam->pageAllocation[i];
      param.superpage *= param.parallelismLevel[i];
    }
    else {
      break;
    }
  }

  panic_if(param.superpageLevel != mask,
           "Invalid superpage configuration detected.");

  param.superpageLevel = (uint8_t)popcount8(mask);

  // Print mapping Information
  debugprint(Log::DebugID::FTL, "Total physical pages %u",
             param.totalPhysicalPages);
  debugprint(Log::DebugID::FTL, "Total logical pages %u",
             param.totalLogicalPages);
  debugprint(Log::DebugID::FTL, "Logical page size %u", param.pageSize);

  // Create events
  eventDoDRAM =
      createEvent([this](uint64_t, uint64_t d) { submitDRAMRequest(d); },
                  "FTL::Mapping::AbstractMapping::eventDoDRAM");
  eventDRAMDone = createEvent([this](uint64_t, uint64_t d) { dramDone(d); },
                              "FTL::Mapping::AbstractMapping::eventDRAMDone");
}

void AbstractMapping::makeSpare(LPN lpn, std::vector<uint8_t> &spare) {
  if (spare.size() != sizeof(LPN)) {
    spare.resize(sizeof(LPN));
  }

  memcpy(spare.data(), &lpn, sizeof(LPN));
}

LPN AbstractMapping::readSpare(std::vector<uint8_t> &spare) {
  LPN lpn = InvalidLPN;

  panic_if(spare.size() < sizeof(LPN), "Empty spare data.");

  memcpy(&lpn, spare.data(), sizeof(LPN));

  return lpn;
}

void AbstractMapping::submitDRAMRequest(uint64_t tag) {
  auto iter = memoryQueue.begin();

  while (iter != memoryQueue.end()) {
    if (iter->tag == tag) {
      break;
    }
  }

  panic_if(iter == memoryQueue.end(), "Memory command not submitted.");

  for (auto &entry : iter->commandList) {
    if (entry.read) {
      object.dram->read(entry.address, entry.size, eventDRAMDone, tag);
    }
    else {
      object.dram->write(entry.address, entry.size, eventDRAMDone, tag);
    }
  }
}

void AbstractMapping::dramDone(uint64_t tag) {
  auto iter = memoryQueue.begin();

  while (iter != memoryQueue.end()) {
    if (iter->tag == tag) {
      break;
    }
  }

  panic_if(iter == memoryQueue.end(), "Memory command not submitted.");

  iter->counter++;

  if (iter->counter == iter->commandList.size()) {
    // Done
    scheduleNow(iter->eid, iter->tag);

    memoryQueue.erase(iter);
  }
}

void AbstractMapping::initialize(AbstractFTL *f,
                                 BlockAllocator::AbstractAllocator *a) {
  pFTL = f;
  allocator = a;
};

Parameter *AbstractMapping::getInfo() {
  return &param;
};

void AbstractMapping::createCheckpoint(std::ostream &out) const noexcept {
  uint64_t size = memoryQueue.size();

  BACKUP_SCALAR(out, size);

  for (auto &iter : memoryQueue) {
    BACKUP_EVENT(out, iter.eid);
    BACKUP_SCALAR(out, iter.tag);
    BACKUP_SCALAR(out, iter.counter);

    size = iter.commandList.size();
    BACKUP_SCALAR(out, size);

    for (auto &entry : iter.commandList) {
      BACKUP_SCALAR(out, entry.address);
      BACKUP_SCALAR(out, entry.size);
      BACKUP_SCALAR(out, entry.read);
    }
  }

  BACKUP_EVENT(out, eventDoDRAM);
  BACKUP_EVENT(out, eventDRAMDone);
}

void AbstractMapping::restoreCheckpoint(std::istream &in) noexcept {
  uint64_t size, size2;

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    Event e;
    uint64_t t;
    uint32_t c;

    RESTORE_EVENT(in, e);
    RESTORE_SCALAR(in, t);
    RESTORE_SCALAR(in, c);

    memoryQueue.emplace_back(e, t, c);

    RESTORE_SCALAR(in, size2);

    for (uint64_t j = 0; j < size; j++) {
      bool r;

      RESTORE_SCALAR(in, t);
      RESTORE_SCALAR(in, c);
      RESTORE_SCALAR(in, r);

      memoryQueue.back().commandList.emplace_back(r, t, c);
    }
  }

  RESTORE_EVENT(in, eventDoDRAM);
  RESTORE_EVENT(in, eventDRAMDone);
}

}  // namespace SimpleSSD::FTL::Mapping
