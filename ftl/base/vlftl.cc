// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/base/vlftl.hh"

#include "ftl/allocator/two_block_allocator.hh"
#include "ftl/mapping/virtually_linked.hh"

namespace SimpleSSD::FTL {

VLFTL::VLFTL(ObjectData &o, CommandManager *c, FIL::FIL *f,
             Mapping::AbstractMapping *m, BlockAllocator::AbstractAllocator *a)
    : BasicFTL(o, c, f, m, a), mergeTag(0) {
  panic_if(
      dynamic_cast<BlockAllocator::TwoBlockAllocator *>(pAllocator) == nullptr,
      "Requires TwoBlockAllocator as block allocator.");
  panic_if(dynamic_cast<Mapping::VirtuallyLinked *>(pMapper) == nullptr,
           "Requires VirtuallyLinked as mapping algorithm.");

  eventDoMerge = createEvent([this](uint64_t, uint64_t) { merge_trigger(); },
                             "FTL::VLFTL::eventDoMerge");
  eventMergeReadDone =
      createEvent([this](uint64_t, uint64_t) { merge_readDone(); },
                  "FTL::VLFTL::eventMergeReadDone");
  eventMergeWriteDone =
      createEvent([this](uint64_t, uint64_t) { merge_writeDone(); },
                  "FTL::VLFTL::eventMergeWriteDone");
}

void VLFTL::triggerGC() {
  if ((pAllocator->checkGCThreshold() || writePendingQueue.size() > 0) &&
      formatInProgress == 0) {
    scheduleNow(eventGCTrigger);
  }

  if (((Mapping::VirtuallyLinked *)pMapper)->triggerMerge(true)) {
    scheduleNow(eventDoMerge);
  }
}

void VLFTL::merge_trigger() {
  mergeTag = ((Mapping::VirtuallyLinked *)pMapper)->getMergeReadCommand();

  auto &cmd = commandManager->getCommand(mergeTag);
  cmd.opcode = Operation::Read;
  cmd.eid = eventMergeReadDone;

  pFIL->submit(mergeTag);
}

void VLFTL::merge_readDone() {
  auto &cmd = commandManager->getCommand(mergeTag);

  cmd.counter++;

  if (cmd.counter == cmd.length) {
    cmd.opcode = Operation::Write;
    cmd.eid = eventMergeWriteDone;
    cmd.counter = 0;

    mergeTag =
        ((Mapping::VirtuallyLinked *)pMapper)->getMergeWriteCommand(mergeTag);

    pFIL->submit(mergeTag);
  }
}

void VLFTL::merge_writeDone() {
  auto &cmd = commandManager->getCommand(mergeTag);

  cmd.counter++;

  if (cmd.counter == 0) {
    ((Mapping::VirtuallyLinked *)pMapper)->destroyMergeCommand(mergeTag);

    if (((Mapping::VirtuallyLinked *)pMapper)->triggerMerge(false)) {
      scheduleNow(eventDoMerge);
    }
  }
}

void VLFTL::createCheckpoint(std::ostream &out) const noexcept {
  BasicFTL::createCheckpoint(out);

  BACKUP_SCALAR(out, mergeTag);

  BACKUP_EVENT(out, eventDoMerge);
  BACKUP_EVENT(out, eventMergeReadDone);
  BACKUP_EVENT(out, eventMergeWriteDone);
}

void VLFTL::restoreCheckpoint(std::istream &in) noexcept {
  BasicFTL::restoreCheckpoint(in);

  RESTORE_SCALAR(in, mergeTag);

  RESTORE_EVENT(in, eventDoMerge);
  RESTORE_EVENT(in, eventMergeReadDone);
  RESTORE_EVENT(in, eventMergeWriteDone);
}

}  // namespace SimpleSSD::FTL
