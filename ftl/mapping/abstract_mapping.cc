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
  eventDRAMRead = createEvent([this](uint64_t, uint64_t) { readDRAM(); },
                              "FTL::Mapping::AbstractMapping::eventDRAMRead");
  eventDRAMWrite = createEvent([this](uint64_t, uint64_t) { writeDRAM(); },
                               "FTL::Mapping::AbstractMapping::eventDRAMWrite");
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

void AbstractMapping::readDRAM() {
  uint64_t oldtag = memoryQueue.front().tag;
  Event eid = memoryQueue.front().eid;

  do {
    auto memreq = std::move(memoryQueue.front());

    memoryQueue.pop_front();
    object.dram->read(memreq.address, memreq.size, eid, oldtag);
  } while (memoryQueue.front().tag == oldtag);
}

void AbstractMapping::writeDRAM() {
  uint64_t oldtag = memoryQueue.front().tag;
  Event eid = memoryQueue.front().eid;

  do {
    auto memreq = std::move(memoryQueue.front());

    memoryQueue.pop_front();
    object.dram->write(memreq.address, memreq.size, eid, oldtag);
  } while (memoryQueue.front().tag == oldtag);
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
    BACKUP_EVENT(out, iter.tag);
    BACKUP_SCALAR(out, iter.address);
    BACKUP_SCALAR(out, iter.size);
  }

  BACKUP_EVENT(out, eventDRAMRead);
  BACKUP_EVENT(out, eventDRAMWrite);
}

void AbstractMapping::restoreCheckpoint(std::istream &in) noexcept {
  uint64_t size;

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    Event e;
    uint64_t t, a, s;

    RESTORE_EVENT(in, e);
    RESTORE_SCALAR(in, t);
    RESTORE_SCALAR(in, a);
    RESTORE_SCALAR(in, s);

    memoryQueue.emplace_back(e, t, a, s);
  }

  RESTORE_EVENT(in, eventDRAMRead);
  RESTORE_EVENT(in, eventDRAMWrite);
}

}  // namespace SimpleSSD::FTL::Mapping
