// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/base/basic_ftl.hh"

namespace SimpleSSD::FTL {

BasicFTL::BasicFTL(ObjectData &o, CommandManager *c, FIL::FIL *f,
                   Mapping::AbstractMapping *m,
                   BlockAllocator::AbstractAllocator *a)
    : AbstractFTL(o, c, f, m, a), gcInProgress(false), formatInProgress(0) {
  // Create events
  eventReadDoFIL = createEvent([this](uint64_t, uint64_t d) { read_doFIL(d); },
                               "FTL::BasicFTL::eventReadDoFIL");
  eventReadFull =
      createEvent([this](uint64_t, uint64_t d) { read_readDone(d); },
                  "FTL::BasicFTL::eventReadFull");
  eventWriteFindDone =
      createEvent([this](uint64_t, uint64_t) { write_findDone(); },
                  "FTL::BasicFTL::eventWriteFindDone");
  eventWriteDoFIL =
      createEvent([this](uint64_t, uint64_t d) { write_doFIL(d); },
                  "FTL::BasicFTL::eventWriteDoFIL");
  eventReadModifyDone =
      createEvent([this](uint64_t, uint64_t) { write_readModifyDone(); },
                  "FTL::BasicFTL::eventReadModifyDone");
  eventWriteDone = createEvent([this](uint64_t, uint64_t) { write_rmwDone(); },
                               "FTL::BasicFTL::eventWriteDone");
  eventInvalidateDoFIL =
      createEvent([this](uint64_t t, uint64_t d) { invalidate_doFIL(t, d); },
                  "FTL::BasicFTL::eventInvalidateDoFIL");
  eventGCTrigger = createEvent([this](uint64_t t, uint64_t) { gc_trigger(t); },
                               "FTL::BasicFTL::eventGCTrigger");
  eventGCGetBlockList =
      createEvent([this](uint64_t, uint64_t) { gc_blockinfo(); },
                  "FTL::BasicFTL::eventGCGetBlockList");
  eventGCRead = createEvent([this](uint64_t, uint64_t) { gc_read(); },
                            "FTL::BasicFTL::eventGCRead");
  eventGCWriteMapping = createEvent([this](uint64_t, uint64_t) { gc_write(); },
                                    "FTL::BasicFTL::eventGCWriteMapping");
  eventGCWrite = createEvent([this](uint64_t, uint64_t) { gc_writeDoFIL(); },
                             "FTL::BasicFTL::eventGCWrite");
  eventGCWriteDone = createEvent([this](uint64_t, uint64_t) { gc_writeDone(); },
                                 "FTL::BasicFTL::eventGCWriteDone");
  eventGCErase = createEvent([this](uint64_t, uint64_t) { gc_erase(); },
                             "FTL::BasicFTL::eventGCErase");
  eventGCEraseDone = createEvent([this](uint64_t, uint64_t) { gc_eraseDone(); },
                                 "FTL::BasicFTL::EventGCEraseDone");
  eventGCDone = createEvent([this](uint64_t t, uint64_t) { gc_done(t); },
                            "FTL::BasicFTL::eventGCDone");

  mappingGranularity = pMapper->mappingGranularity();
  mergeReadModifyWrite = readConfigBoolean(Section::FlashTranslation,
                                           Config::Key::MergeReadModifyWrite);
  allowPageRead = readConfigBoolean(Section::FlashTranslation,
                                    Config::Key::AllowPageLevelRead);
}

BasicFTL::~BasicFTL() {}

void BasicFTL::read_find(Command &cmd) {
  Command *pcmd = &cmd;

  // If we don't allow page-level read,
  if (!allowPageRead) {
    // Check this request is aligned to mapping granularity
    LPN alignedBegin = cmd.offset / mappingGranularity * mappingGranularity;
    LPN alignedEnd = alignedBegin + DIVCEIL(cmd.length, mappingGranularity) *
                                        mappingGranularity;

    if (alignedBegin != cmd.offset || cmd.offset + cmd.length != alignedEnd) {
      // If not aligned, we need to read full-sized superpage
      uint64_t tag = makeFTLCommandTag();

      commandManager->createICLRead(tag, eventReadFull, alignedBegin,
                                    alignedEnd - alignedBegin, 0);

      auto &rcmd = commandManager->getCommand(tag);

      // Store original command in counter variable
      rcmd.counter = cmd.tag;

      // Override pcmd
      pcmd = &rcmd;
    }
  }

  pMapper->readMapping(*pcmd, eventReadDoFIL);
}

void BasicFTL::read_doFIL(uint64_t tag) {
  // Now we have PPN
  pFIL->submit(tag);
}

void BasicFTL::read_readDone(uint64_t tag) {
  auto &rcmd = commandManager->getCommand(tag);
  auto &cmd = commandManager->getCommand(rcmd.counter);

  // Full-sized read done
  scheduleNow(cmd.eid, cmd.tag);

  commandManager->destroyCommand(tag);
}

void BasicFTL::write_find(Command &cmd) {
  CPU::Function fstat = CPU::initFunction();

  // Check GC threshold for On-demand GC
  if (pAllocator->checkGCThreshold() && formatInProgress == 0) {
    scheduleNow(eventGCTrigger);
  }

  // Check this request is aligned to mapping granularity
  LPN alignedBegin = cmd.offset / mappingGranularity * mappingGranularity;
  LPN alignedEnd = alignedBegin +
                   DIVCEIL(cmd.length, mappingGranularity) * mappingGranularity;

  if (alignedBegin != cmd.offset || cmd.offset + cmd.length != alignedEnd ||
      cmd.subCommandList.front().skipFront > 0 ||
      cmd.subCommandList.back().skipEnd > 0) {
    bool merged = false;

    // Not aligned - read-modify-write
    warn_if(alignedBegin + mappingGranularity != alignedEnd,
            "Merge might fail. (I/O size > one mapping granularity)");

    // 1. Check merge
    if (mergeReadModifyWrite) {
      for (auto entry = rmwList.begin(); entry != rmwList.end(); ++entry) {
        // Not written yet
        if (!entry->writePending) {
          if (entry->offset == alignedBegin) {
            merged = true;

            // Merge with this request
            ReadModifyWriteContext ctx(cmd.eid, cmd.tag);

            // Record how many completion is needed
            ctx.writeTag = cmd.length;

            rmwList.emplace(++entry, ctx);

            break;
          }
        }
      }
    }

    if (!merged) {
      // 2. Create read request
      ReadModifyWriteContext ctx(cmd.eid, cmd.tag);

      ctx.offset = alignedBegin;
      ctx.length = alignedEnd - alignedBegin;

      uint64_t tag = makeFTLCommandTag();

      commandManager->createICLRead(tag, eventReadModifyDone, ctx.offset,
                                    ctx.length, 0);

      auto &rcmd = commandManager->getCommand(tag);

      fstat += pMapper->readMapping(rcmd);

      // 2-1. Prepare exclude [cmd.offset, cmd.offset + cmd.length)
      LPN excludeBegin = cmd.offset;
      LPN excludeEnd = cmd.offset + cmd.length;

      // 2-1-1. We need to include cmd.offset when skipFront > 0
      if (cmd.subCommandList.front().skipFront > 0) {
        excludeBegin++;
      }

      // 2-1-2. We need to include cmd.offset + cmd.length - 1 when skipEnd > 0
      if (cmd.subCommandList.back().skipEnd > 0) {
        excludeEnd--;
      }

      // 2-1-3. Exclude
      for (auto &scmd : rcmd.subCommandList) {
        if (excludeBegin <= scmd.lpn && scmd.lpn < excludeEnd) {
          scmd.lpn = InvalidLPN;
          scmd.ppn = InvalidPPN;

          rcmd.counter++;
        }
      }

      ctx.readTag = tag;

      // 3. Create write request [alignedBegin, alignedEnd)
      tag = makeFTLCommandTag();

      commandManager->createICLWrite(tag, eventWriteDone, ctx.offset,
                                     ctx.length, 0, 0, 0);

      auto &wcmd = commandManager->getCommand(tag);

      wcmd.counter = mappingGranularity - cmd.length;

      fstat += pMapper->writeMapping(wcmd);

      ctx.writeTag = tag;

      rmwList.emplace_back(ctx);
    }
  }
  else {
    pMapper->writeMapping(cmd, eventWriteDoFIL);

    return;
  }

  scheduleFunction(CPU::CPUGroup::FlashTranslationLayer, eventWriteFindDone,
                   fstat);
}

void BasicFTL::write_findDone() {
  if (rmwList.size() > 0) {
    auto &job = rmwList.front();

    if (!job.readPending) {
      job.readPending = true;

      pFIL->submit(job.readTag);
    }
  }
}

void BasicFTL::write_doFIL(uint64_t tag) {
  // Now we have PPN
  pFIL->submit(tag);
}

void BasicFTL::write_readModifyDone() {
  auto &job = rmwList.front();
  auto &rcmd = commandManager->getCommand(job.readTag);

  rcmd.counter++;

  if (rcmd.counter == rcmd.length) {
    job.writePending = true;

    pFIL->submit(job.writeTag);
  }
}

void BasicFTL::write_rmwDone() {
  auto &job = rmwList.front();
  LPN completed = 0;

  if (job.readTag > 0) {
    auto &wcmd = commandManager->getCommand(job.writeTag);

    for (auto &iter : wcmd.subCommandList) {
      if (iter.status == Status::Done) {
        completed++;
      }
      else {
        completed++;

        iter.status = Status::Done;

        break;
      }
    }

    if (wcmd.counter < completed) {
      scheduleNow(job.originalEvent, job.originalTag);
    }
  }
  else {
    // We stored original length (cmd.length) to writeTag
    if (job.writeTag-- > 0) {
      // Complete cmd.length count
      scheduleNow(job.originalEvent, job.originalTag);
      scheduleNow(eventWriteDone);
    }
    else {
      // We are done
      completed = mappingGranularity;
    }
  }

  if (completed == mappingGranularity) {
    if (job.readTag > 0) {
      commandManager->destroyCommand(job.readTag);
      commandManager->destroyCommand(job.writeTag);
    }

    rmwList.pop_front();

    if (rmwList.size() > 0) {
      // Check merged
      if (rmwList.front().readTag == 0) {
        // Call this function again
        scheduleNow(eventWriteDone);
      }
      else {
        scheduleNow(eventWriteFindDone);
      }
    }
  }
}

void BasicFTL::invalidate_find(Command &cmd) {
  pMapper->invalidateMapping(cmd, eventInvalidateDoFIL);
}

void BasicFTL::invalidate_doFIL(uint64_t now, uint64_t tag) {
  auto &cmd = commandManager->getCommand(tag);

  if (cmd.opcode == Operation::Trim) {
    // Complete here (not erasing blocks)
    scheduleNow(cmd.eid, tag);
  }
  else if (formatInProgress == 0) {
    // Erase block here
    formatInProgress = 100;  // 100% remain
    fctx.eid = cmd.eid;
    fctx.data = tag;

    // We already have old PPN in cmd.subCommandList
    // Copy them
    gcBlockList.clear();

    for (auto &scmd : cmd.subCommandList) {
      gcBlockList.emplace_back(scmd.ppn);
    }

    debugprint(Log::DebugID::FTL, "Fmt   | Immediate erase | %u blocks",
               gcBlockList.size());

    gcBeginAt = now;
  }
  else {
    // TODO: Handle this case
    scheduleNow(cmd.eid, tag);
  }
}

void BasicFTL::gc_trigger(uint64_t now) {
  gcInProgress = true;
  formatInProgress = 100;
  gcBeginAt = now;

  // Get block list from allocator
  pAllocator->getVictimBlocks(gcBlockList, eventGCGetBlockList);

  debugprint(Log::DebugID::FTL, "GC    | On-demand | %u blocks",
             gcBlockList.size());
}

void BasicFTL::gc_blockinfo() {
  if (LIKELY(gcBlockList.size() > 0)) {
    PPN block = gcBlockList.front();

    gcBlockList.pop_front();
    gcCopyList.blockID = block;

    pMapper->getCopyList(gcCopyList, eventGCRead);
    gcCopyList.resetIterator();
  }
  else {
    scheduleNow(eventGCDone);
  }
}

void BasicFTL::gc_read() {
  // Find valid page
  if (LIKELY(!gcCopyList.isEnd())) {
    // Update completion info
    auto &cmd = commandManager->getCommand(*gcCopyList.iter);

    cmd.eid = eventGCWriteMapping;
    cmd.opcode = Operation::Read;
    cmd.counter = 0;

    debugprint(Log::DebugID::FTL,
               "GC    | Read  | PPN %" PRIx64 "h + %" PRIx64 "h",
               cmd.subCommandList.front().ppn, cmd.length);

    pFIL->submit(cmd.tag);

    return;
  }

  // Do erase
  auto &cmd = commandManager->getCommand(gcCopyList.eraseTag);

  cmd.eid = eventGCErase;
  cmd.opcode = Operation::Erase;

  pFIL->submit(gcCopyList.eraseTag);
}

void BasicFTL::gc_write() {
  auto &cmd = commandManager->getCommand(*gcCopyList.iter);

  cmd.counter++;

  if (cmd.counter == cmd.length) {
    PPN old = cmd.subCommandList.front().ppn;

    pMapper->writeMapping(cmd, eventGCWrite);

    debugprint(Log::DebugID::FTL,
               "GC    | Write | PPN %" PRIx64 "h (LPN %" PRIx64
               "h) -> PPN %" PRIx64 "h + %" PRIx64 "h",
               old, cmd.offset, cmd.subCommandList.front().ppn, cmd.length);
  }
}

void BasicFTL::gc_writeDoFIL() {
  // Update completion info
  auto &cmd = commandManager->getCommand(*gcCopyList.iter);

  cmd.eid = eventGCWriteDone;
  cmd.opcode = Operation::Write;
  cmd.counter = 0;

  pFIL->submit(cmd.tag);
}

void BasicFTL::gc_writeDone() {
  auto &cmd = commandManager->getCommand(*gcCopyList.iter);

  cmd.counter++;

  if (cmd.counter == cmd.length) {
    // Next!
    ++gcCopyList.iter;

    scheduleNow(eventGCRead);
  }
}

void BasicFTL::gc_erase() {
  pAllocator->reclaimBlocks(gcCopyList.blockID, eventGCEraseDone);
}

void BasicFTL::gc_eraseDone() {
  scheduleNow(eventGCGetBlockList);

  pMapper->releaseCopyList(gcCopyList);
}

void BasicFTL::gc_done(uint64_t now) {
  formatInProgress = 0;

  if (gcInProgress) {
    debugprint(Log::DebugID::FTL,
               "GC    | On-demand | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")",
               gcBeginAt, now, now - gcBeginAt);
  }
  else {
    debugprint(Log::DebugID::FTL,
               "Fmt   | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")", gcBeginAt,
               now, now - gcBeginAt);

    // This was format
    scheduleNow(fctx.eid, fctx.data);
  }

  gcInProgress = false;
}

void BasicFTL::submit(uint64_t tag) {
  auto &cmd = commandManager->getCommand(tag);

  switch (cmd.opcode) {
    case Operation::Read:
      read_find(cmd);

      break;
    case Operation::Write:
      write_find(cmd);

      break;
    case Operation::Trim:
    case Operation::Format:
      invalidate_find(cmd);

      break;
    default:
      panic("Unexpected opcode.");

      break;
  }
}

bool BasicFTL::isGC() {
  return gcInProgress;
}

uint8_t BasicFTL::isFormat() {
  if (gcInProgress) {
    return 0;
  }
  else {
    return formatInProgress;
  }
}

void BasicFTL::getStatList(std::vector<Stat> &, std::string) noexcept {}

void BasicFTL::getStatValues(std::vector<double> &) noexcept {}

void BasicFTL::resetStatValues() noexcept {}

void BasicFTL::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, mergeReadModifyWrite);
  BACKUP_SCALAR(out, gcInProgress);
  BACKUP_SCALAR(out, gcBeginAt);

  BACKUP_SCALAR(out, formatInProgress);
  BACKUP_EVENT(out, fctx.eid);
  BACKUP_SCALAR(out, fctx.data);

  gcCopyList.createCheckpoint(out);

  uint64_t size = gcBlockList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : gcBlockList) {
    BACKUP_SCALAR(out, iter);
  }

  BACKUP_EVENT(out, eventReadDoFIL);
  BACKUP_EVENT(out, eventReadFull);
  BACKUP_EVENT(out, eventWriteDoFIL);
  BACKUP_EVENT(out, eventInvalidateDoFIL);
  BACKUP_EVENT(out, eventGCTrigger);
  BACKUP_EVENT(out, eventGCGetBlockList);
  BACKUP_EVENT(out, eventGCRead);
  BACKUP_EVENT(out, eventGCWriteMapping);
  BACKUP_EVENT(out, eventGCWrite);
  BACKUP_EVENT(out, eventGCWriteDone);
  BACKUP_EVENT(out, eventGCErase);
  BACKUP_EVENT(out, eventGCEraseDone);
  BACKUP_EVENT(out, eventGCDone);
  BACKUP_EVENT(out, eventWriteFindDone);
  BACKUP_EVENT(out, eventReadModifyDone);
  BACKUP_EVENT(out, eventWriteDone);
}

void BasicFTL::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, mergeReadModifyWrite);
  RESTORE_SCALAR(in, gcInProgress);
  RESTORE_SCALAR(in, gcBeginAt);

  RESTORE_SCALAR(in, formatInProgress);
  RESTORE_EVENT(in, fctx.eid);
  RESTORE_SCALAR(in, fctx.data);

  gcCopyList.restoreCheckpoint(in);

  uint64_t size;
  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    PPN ppn;

    RESTORE_SCALAR(in, ppn);

    gcBlockList.emplace_back(ppn);
  }

  RESTORE_EVENT(in, eventReadDoFIL);
  RESTORE_EVENT(in, eventReadFull);
  RESTORE_EVENT(in, eventWriteDoFIL);
  RESTORE_EVENT(in, eventInvalidateDoFIL);
  RESTORE_EVENT(in, eventGCTrigger);
  RESTORE_EVENT(in, eventGCGetBlockList);
  RESTORE_EVENT(in, eventGCRead);
  RESTORE_EVENT(in, eventGCWriteMapping);
  RESTORE_EVENT(in, eventGCWrite);
  RESTORE_EVENT(in, eventGCWriteDone);
  RESTORE_EVENT(in, eventGCErase);
  RESTORE_EVENT(in, eventGCEraseDone);
  RESTORE_EVENT(in, eventGCDone);
  RESTORE_EVENT(in, eventWriteFindDone);
  RESTORE_EVENT(in, eventReadModifyDone);
  RESTORE_EVENT(in, eventWriteDone);
}

}  // namespace SimpleSSD::FTL
