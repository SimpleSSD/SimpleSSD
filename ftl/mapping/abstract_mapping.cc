// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/mapping/abstract_mapping.hh"

namespace SimpleSSD::FTL::Mapping {

AbstractMapping::AbstractMapping(ObjectData &o, FTLObjectData &fo)
    : Object(o), ftlobject(fo), memoryTag(0) {
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
  debugprint(Log::DebugID::FTL, "Total physical pages %" PRIu64,
             param.totalPhysicalPages);
  debugprint(Log::DebugID::FTL, "Total logical pages %" PRIu64,
             param.totalLogicalPages);
  debugprint(Log::DebugID::FTL, "Logical page size %u", param.pageSize);

  resetStatValues();

  eventMemoryDone =
      createEvent([this](uint64_t, uint64_t d) { handleMemoryCommand(d); },
                  "FTL::Mapping::AbstractMapping::eventMemoryDone");
}

void AbstractMapping::handleMemoryCommand(uint64_t tag) {
  auto iter = memoryCommandList.find(tag);

  panic_if(iter == memoryCommandList.end(), "Unexpected memory command.");

  // Issue dram
  if (iter->second.cmdList.size() > 0) {
    auto &cmd = iter->second.cmdList.front();

    if (cmd.read) {
      object.memory->read(cmd.address, cmd.size, eventMemoryDone, tag);
    }
    else {
      object.memory->write(cmd.address, cmd.size, eventMemoryDone, tag);
    }

    iter->second.cmdList.pop_front();
  }
  else {
    // Handle completion
    if (iter->second.eid != InvalidEventID) {
      scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, iter->second.eid,
                       iter->second.data, iter->second.fstat);
    }

    memoryCommandList.erase(iter);
  }
}

void AbstractMapping::insertMemoryAddress(bool read, uint64_t address,
                                          uint32_t size, bool en) {
  if (en) {
    pendingMemoryAccess.emplace_back(read, address, size);
  }
}

void AbstractMapping::requestMemoryAccess(Event eid, uint64_t data,
                                          CPU::Function &fstat) {
  auto memtag = makeMemoryTag();
  auto ret = memoryCommandList.emplace(memtag, CommandList());

  panic_if(!ret.second, "Memory tag conflict.");

  ret.first->second.eid = eid;
  ret.first->second.data = data;
  ret.first->second.fstat = std::move(fstat);
  ret.first->second.cmdList = std::move(pendingMemoryAccess);

  handleMemoryCommand(memtag);
}

void AbstractMapping::initialize() {}

const Parameter *AbstractMapping::getInfo() {
  return &param;
};

void AbstractMapping::getStatList(std::vector<Stat> &list,
                                  std::string prefix) noexcept {
  list.emplace_back(prefix + "count.read", "Total read translation requests");
  list.emplace_back(prefix + "count.write", "Total write translation requests");
  list.emplace_back(prefix + "count.invalidate", "Total invalidate requests");
  list.emplace_back(prefix + "count.page.read",
                    "Total read translation requests");
  list.emplace_back(prefix + "count.page.write",
                    "Total write translation requests");
  list.emplace_back(prefix + "count.page.invalidate",
                    "Total invalidate requests");
}

void AbstractMapping::getStatValues(std::vector<double> &values) noexcept {
  values.push_back((double)requestedReadCount);
  values.push_back((double)requestedWriteCount);
  values.push_back((double)requestedInvalidateCount);
  values.push_back((double)readLPNCount);
  values.push_back((double)writeLPNCount);
  values.push_back((double)invalidateLPNCount);
}

void AbstractMapping::resetStatValues() noexcept {
  requestedReadCount = 0;
  requestedWriteCount = 0;
  requestedInvalidateCount = 0;
  readLPNCount = 0;
  writeLPNCount = 0;
  invalidateLPNCount = 0;
}

void AbstractMapping::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, requestedReadCount);
  BACKUP_SCALAR(out, requestedWriteCount);
  BACKUP_SCALAR(out, requestedInvalidateCount);
  BACKUP_SCALAR(out, readLPNCount);
  BACKUP_SCALAR(out, writeLPNCount);
  BACKUP_SCALAR(out, invalidateLPNCount);

  uint64_t size = pendingMemoryAccess.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : pendingMemoryAccess) {
    BACKUP_SCALAR(out, iter.read);
    BACKUP_SCALAR(out, iter.address);
    BACKUP_SCALAR(out, iter.size);
  }

  size = memoryCommandList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : memoryCommandList) {
    BACKUP_SCALAR(out, iter.first);

    BACKUP_EVENT(out, iter.second.eid);
    BACKUP_SCALAR(out, iter.second.data);

    BACKUP_SCALAR(out, iter.second.fstat);

    size = iter.second.cmdList.size();
    BACKUP_SCALAR(out, size);

    for (auto &iiter : iter.second.cmdList) {
      BACKUP_SCALAR(out, iiter.read);
      BACKUP_SCALAR(out, iiter.address);
      BACKUP_SCALAR(out, iiter.size);
    }
  }

  BACKUP_SCALAR(out, memoryTag);

  BACKUP_EVENT(out, eventMemoryDone);
}

void AbstractMapping::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, requestedReadCount);
  RESTORE_SCALAR(in, requestedWriteCount);
  RESTORE_SCALAR(in, requestedInvalidateCount);
  RESTORE_SCALAR(in, readLPNCount);
  RESTORE_SCALAR(in, writeLPNCount);
  RESTORE_SCALAR(in, invalidateLPNCount);

  uint64_t size;
  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    bool r;
    uint64_t a;
    uint32_t s;

    RESTORE_SCALAR(in, r);
    RESTORE_SCALAR(in, a);
    RESTORE_SCALAR(in, s);

    pendingMemoryAccess.emplace_back(r, a, s);
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    uint64_t f;
    CommandList ctx;

    RESTORE_SCALAR(in, f);

    RESTORE_EVENT(in, ctx.eid);
    RESTORE_SCALAR(in, ctx.data);

    RESTORE_SCALAR(in, ctx.fstat);

    uint64_t ssize;
    RESTORE_SCALAR(in, ssize);

    for (uint64_t j = 0; j < ssize; j++) {
      bool r;
      uint64_t a;
      uint32_t s;

      RESTORE_SCALAR(in, r);
      RESTORE_SCALAR(in, a);
      RESTORE_SCALAR(in, s);

      ctx.cmdList.emplace_back(r, a, s);
    }

    memoryCommandList.emplace(f, ctx);
  }

  RESTORE_SCALAR(in, memoryTag);

  RESTORE_EVENT(in, eventMemoryDone);
}

}  // namespace SimpleSSD::FTL::Mapping
