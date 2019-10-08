// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/queue_arbitrator.hh"

#include "hil/nvme/controller.hh"

namespace SimpleSSD::HIL::NVMe {

#define debugprint_ctrl(format, ...)                                           \
  {                                                                            \
    debugprint(Log::DebugID::HIL_NVMe, "CTRL %-3d | " format, controllerID,    \
               ##__VA_ARGS__);                                                 \
  }

SQContext::SQContext()
    : commandID(0),
      sqID(0),
      cqID(0),
      sqHead(0),
      useSGL(false),
      dispatched(false),
      completed(false) {}

void SQContext::update(uint16_t sqid, uint16_t cqid, uint16_t sqhead) noexcept {
  sqID = sqid;
  cqID = cqid;
  sqHead = sqhead;
}

void SQContext::update() noexcept {
  commandID = entry.dword0.commandID;

  if (entry.dword0.psdt == 0x00) {
    useSGL = false;
  }
  else {
    useSGL = true;
  }
}

SQEntry *SQContext::getData() noexcept {
  return &entry;
}

uint32_t SQContext::getID() noexcept {
  return ((uint32_t)sqID << 16) | commandID;
}

uint16_t SQContext::getSQID() noexcept {
  return sqID;
}

uint16_t SQContext::getCQID() noexcept {
  return cqID;
}

bool SQContext::isSGL() noexcept {
  return useSGL;
}

CQContext::CQContext() : cqID(0) {}

void CQContext::update(SQContext *sqe) noexcept {
  cqID = sqe->cqID;
  entry.dword2.sqHead = sqe->sqHead;
  entry.dword2.sqID = sqe->sqID;
  entry.dword3.commandID = sqe->entry.dword0.commandID;

  sqe->completed = true;
}

bool CQContext::isSuccess() noexcept {
  return entry.dword3.sct == (uint8_t)StatusType::GenericCommandStatus &&
         entry.dword3.sc == (uint8_t)GenericCommandStatusCode::Success;
}

CQEntry *CQContext::getData() {
  return &entry;
}

uint32_t CQContext::getID() noexcept {
  return ((uint32_t)entry.dword2.sqID << 16) | entry.dword3.commandID;
}

uint16_t CQContext::getSQID() noexcept {
  return entry.dword2.sqID;
}

uint16_t CQContext::getCQID() noexcept {
  return cqID;
}

Arbitrator::Arbitrator(ObjectData &o, ControllerData *c)
    : Object(o),
      controller(c),
      controllerID(c->controller->getControllerID()),
      mode(Arbitration::RoundRobin),
      shutdownReserved(false) {
  // Read config
  period = readConfigUint(Section::HostInterface, Config::Key::WorkInterval);
  internalQueueSize =
      readConfigUint(Section::HostInterface, Config::Key::RequestQueueSize);
  param.hpw = (uint8_t)(
      readConfigUint(Section::HostInterface, Config::Key::NVMeWRRHigh) - 1);
  param.mpw = (uint8_t)(
      readConfigUint(Section::HostInterface, Config::Key::NVMeWRRMedium) - 1);
  param.lpw = 0;
  auto burst = (uint8_t)log2(internalQueueSize);

  if (burst >= 7) {
    burst = 7;
  }

  param.ab = burst;

  // Allocate queues
  sqSize =
      (uint16_t)readConfigUint(Section::HostInterface, Config::Key::NVMeMaxSQ);
  cqSize =
      (uint16_t)readConfigUint(Section::HostInterface, Config::Key::NVMeMaxCQ);

  cqList = (CQueue **)calloc(cqSize, sizeof(CQueue *));
  sqList = (SQueue **)calloc(sqSize, sizeof(SQueue *));

  // Create events
  work = createEvent([this](uint64_t t) { collect(t); },
                     "HIL::NVMe::Arbitrator::work");
  eventCompDone = createEvent([this](uint64_t) { completion_done(); },
                              "HIL::NVMe::Arbitrator::eventCompDone");
  eventCollect = createEvent([this](uint64_t) { collect_done(); },
                             "HIL::NVMe::Arbitrator::eventCollect");

  // Not running!
  run = false;
  running = false;
}

Arbitrator::~Arbitrator() {
  // Deallocate queues
  for (auto i = 0u; i < cqSize; i++) {
    if (cqList[i]) {
      delete cqList[i];
    }
  }

  for (auto i = 0u; i < sqSize; i++) {
    if (sqList[i]) {
      delete sqList[i];
    }
  }

  free(cqList);
  free(sqList);
}

void Arbitrator::enable(bool r) {
  run = r;

  if (run) {
    schedule(work, getTick());
  }
  else {
    deschedule(work);
  }
}

void Arbitrator::setMode(Arbitration newMode) {
  mode = newMode;
}

void Arbitrator::ringSQ(uint16_t qid, uint16_t tail) {
  auto sq = sqList[qid];

  panic_if(!sq, "Access to unintialized submission queue.");

  auto oldtail = sq->getTail();
  auto oldcount = sq->getItemCount();

  sq->setTail(tail);

  debugprint_ctrl(
      "SQ %-5d| Submission Queue Tail Doorbell | Item count in queue "
      "%d -> %d | head %d | tail %d -> %d",
      qid, oldcount, sq->getItemCount(), sq->getHead(), oldtail, sq->getTail());
}

void Arbitrator::ringCQ(uint16_t qid, uint16_t head) {
  auto cq = cqList[qid];

  panic_if(!cq, "Access to unintialized completion queue.");

  auto oldhead = cq->getHead();
  auto oldcount = cq->getItemCount();

  cq->setHead(head);

  debugprint_ctrl(
      "CQ %-5d| Completion Queue Head Doorbell | Item count in queue "
      "%d -> %d | head %d -> %d | tail %d",
      qid, oldcount, cq->getItemCount(), oldhead, cq->getHead(), cq->getTail());

  if (cq->getItemCount() == 0 && cq->interruptEnabled()) {
    controller->interruptManager->postInterrupt(cq->getInterruptVector(),
                                                false);
  }
}

SQContext *Arbitrator::dispatch() {
dispatch_again:
  if (LIKELY(requestQueue.size() > 0)) {
    auto entry = (SQContext *)requestQueue.front();

    entry->dispatched = true;

    auto ret = dispatchedQueue.push_back(entry->getID(), entry);
    requestQueue.pop_front();

    if (UNLIKELY(!ret)) {
      // Command ID duplication
      abortCommand(entry, StatusType::CommandSpecificStatus,
                   GenericCommandStatusCode::CommandIDConflict);

      delete entry;

      // Retry
      goto dispatch_again;
    }

    return entry;
  }

  return nullptr;
}

void Arbitrator::reserveShutdown() {
  shutdownReserved = true;
}

void Arbitrator::createAdminCQ(uint64_t base, uint16_t size, Event eid) {
  auto dmaEngine =
      new PRPEngine(object, controller->dma, controller->memoryPageSize);

  dmaEngine->initQueue(base, size * 16, true, eid);

  if (cqList[0]) {
    delete cqList[0];
  }

  cqList[0] = new CQueue(object, 0, size, 0, true);
  cqList[0]->setBase(dmaEngine, 16);  // Always 16 bytes stride

  debugprint_ctrl("CQ 0    | CREATE | Entry size %d", size);
}

void Arbitrator::createAdminSQ(uint64_t base, uint16_t size, Event eid) {
  auto dmaEngine =
      new PRPEngine(object, controller->dma, controller->memoryPageSize);

  dmaEngine->initQueue(base, size * 64, true, eid);

  if (sqList[0]) {
    delete sqList[0];
  }

  sqList[0] = new SQueue(object, 0, size, 0, QueuePriority::Urgent);
  sqList[0]->setBase(dmaEngine, 64);  // Always 64 bytes stride

  debugprint_ctrl("SQ 0    | CREATE | Entry size %d", size);
}

ArbitrationData *Arbitrator::getArbitrationData() {
  return &param;
}

void Arbitrator::applyArbitrationData() {
  // Ignore unlimited burst
  if (param.ab >= 7) {
    param.ab = 7;
  }

  // Update queue size
  internalQueueSize = 1ull << param.ab;

  // Update config
  writeConfigUint(Section::HostInterface, Config::Key::RequestQueueSize,
                  internalQueueSize);
  writeConfigUint(Section::HostInterface, Config::Key::NVMeWRRHigh,
                  param.hpw + 1);
  writeConfigUint(Section::HostInterface, Config::Key::NVMeWRRMedium,
                  param.mpw + 1);
}

void Arbitrator::requestIOQueues(uint16_t &nsq, uint16_t &ncq) {
  // Both values are zero-based
  if (nsq + 2 > sqSize) {
    nsq = sqSize - 2;
  }
  if (ncq + 2 > cqSize) {
    ncq = cqSize - 2;
  }
}

uint8_t Arbitrator::createIOSQ(uint64_t base, uint16_t id, uint16_t size,
                               uint16_t cqid, uint8_t pri, bool pc,
                               uint16_t setid, Event eid) {
  uint64_t sqEntrySize, cqEntrySize;

  if (UNLIKELY(!cqList[cqid])) {
    return 2u;
  }

  if (UNLIKELY(sqList[id])) {
    return 1u;
  }

  controller->controller->getQueueStride(sqEntrySize, cqEntrySize);

  auto sq = new SQueue(object, id, size, cqid, (QueuePriority)pri);

  panic_if(!sq, "Failed to allocate completion queue.");

  sqList[id] = sq;

  auto dma = new PRPEngine(object, controller->dma, controller->memoryPageSize);

  dma->initQueue(base, size * sqEntrySize, pc, eid);

  sq->setBase(dma, sqEntrySize);

  debugprint_ctrl(
      "SQ %-4d | CREATE | Size %u | CQ %u | Priority %u | Set ID %u", id, size,
      cqid, pri, setid);

  return 0;
}

uint8_t Arbitrator::createIOCQ(uint64_t base, uint16_t id, uint16_t size,
                               uint16_t iv, bool ien, bool pc, Event eid) {
  uint64_t sqEntrySize, cqEntrySize;

  if (UNLIKELY(cqList[id])) {
    return 1u;
  }

  controller->controller->getQueueStride(sqEntrySize, cqEntrySize);

  auto cq = new CQueue(object, id, size, iv, ien);

  panic_if(!cq, "Failed to allocate completion queue.");

  cqList[id] = cq;

  auto dma = new PRPEngine(object, controller->dma, controller->memoryPageSize);

  dma->initQueue(base, size * cqEntrySize, pc, eid);

  cq->setBase(dma, cqEntrySize);

  debugprint_ctrl("CQ %-4d | CREATE | Size %u | IV %u", id, size, iv);

  return 0;
}

uint8_t Arbitrator::deleteIOSQ(uint16_t id, Event eid) {
  auto sq = sqList[id];
  bool aborted = false;

  panic_if(id == 0, "Cannot delete admin SQ.");

  if (UNLIKELY(!sq || abortSQList.find(id) != abortSQList.end())) {
    return 1u;
  }

  // Abort all commands submitted from current sq
  for (auto iter = requestQueue.begin(); iter != requestQueue.end();) {
    auto entry = (SQContext *)iter.getValue();

    panic_if(!entry, "Corrupted request queue.");

    if (entry->getSQID() == id) {
      aborted = true;

      // Abort command
      abortCommand(entry, StatusType::GenericCommandStatus,
                   GenericCommandStatusCode::Abort_SQDeletion);

      iter = requestQueue.erase(iter);
      delete entry;
    }
    else {
      ++iter;
    }
  }

  // Push current event to wait all aborted commands to complete
  abortSQList.emplace(std::make_pair(id, eid));

  if (!aborted) {
    // We don't have aborted commands
    abort_SQDone();
  }

  return 0;
}

uint8_t Arbitrator::deleteIOCQ(uint16_t id) {
  auto cq = cqList[id];

  panic_if(id == 0, "Cannot delete admin CQ.");

  if (UNLIKELY(!cq)) {
    return 1u;
  }

  // Check corresponding SQ exists
  for (uint16_t i = 0; i < sqSize; i++) {
    if (sqList[i] && sqList[i]->getCQID() == id) {
      return 3u;
    }
  }

  // Don't need to check pending requests in CQ
  // All commands are aborted from SQ -> SQ deletion -> Delete I/O SQ completion
  debugprint_ctrl("CQ %-4d | DELETE", id);

  delete cq;
  cqList[id] = nullptr;

  return 0;
}

uint8_t Arbitrator::abortCommand(uint16_t sqid, uint16_t cid, Event eid) {
  uint32_t id = ((uint32_t)sqid << 16) | cid;

  // Find command
  auto iter = (SQContext *)requestQueue.find(id);

  if (iter) {
    // We have command and will aborted
    abortCommand(iter, StatusType::GenericCommandStatus,
                 GenericCommandStatusCode::Abort_Requested);

    requestQueue.erase(id);
    delete iter;

    abortCommandList.emplace(std::make_pair(id, eid));

    return 0u;
  }
  else {
    iter = (SQContext *)dispatchedQueue.find(id);

    if (!iter) {
      // No such command found (may be completed?)
      return 1u;
    }

    // Pending command
    return 2u;
  }
}

void Arbitrator::complete(CQContext *cqe, bool ignore) {
  auto cq = cqList[cqe->getCQID()];

  panic_if(!cq, "Completion to invalid completion queue.");

  // Find corresponding SQContext
  if (UNLIKELY(!ignore)) {
    auto sqe = (SQContext *)dispatchedQueue.find(cqe->getID());

    panic_if(!sqe, "Failed to find corresponding submission entry.");
    panic_if(!sqe->completed, "Corresponding submission entry not completed.");

    // Remove SQContext
    dispatchedQueue.erase(sqe->getID());
    delete sqe;
  }

  // Insert to completion queue
  completionQueue.push_back(cqe);

  // Write entry to CQ
  cq->setData(cqe->getData(), eventCompDone);
}

void Arbitrator::completion_done() {
  auto cqe = completionQueue.front();
  auto cq = cqList[cqe->getCQID()];

  auto id = cqe->getID();

  completionQueue.pop_front();

  if (cq->interruptEnabled()) {
    controller->interruptManager->postInterrupt(cq->getInterruptVector(), true);
  }

  // Remove CQContext
  delete cqe;

  // Pending abort events?
  abort_SQDone();
  abort_CommandDone(id);

  // Shutdown?
  if (UNLIKELY(shutdownReserved && checkShutdown())) {
    finishShutdown();
  }
}

void Arbitrator::abort_SQDone() {
  if (LIKELY(abortSQList.size() == 0)) {
    return;
  }

  std::map<uint16_t, uint32_t> countList;

  // Check pending queue
  for (auto iter = dispatchedQueue.begin(); iter != dispatchedQueue.end();
       ++iter) {
    auto id = ((SQContext *)iter.getValue())->getSQID();

    auto count = countList.emplace(std::make_pair(id, 0));
    count.first->second++;
  }

  // Check completion queue
  for (auto iter = completionQueue.begin(); iter != completionQueue.end();
       ++iter) {
    auto id = (*iter)->getSQID();

    auto count = countList.emplace(std::make_pair(id, 0));
    count.first->second++;
  }

  // Call events that completed
  for (auto iter = abortSQList.begin(); iter != abortSQList.end();) {
    auto count = countList.find(iter->first);

    if (count == countList.end()) {
      debugprint_ctrl("SQ %-4d | DELETE", iter->first);

      delete sqList[iter->first];
      sqList[iter->first] = nullptr;

      schedule(iter->second, getTick());

      iter = abortSQList.erase(iter);
    }
    else {
      ++iter;
    }
  }
}

void Arbitrator::abort_CommandDone(uint32_t id) {
  if (LIKELY(abortCommandList.size() == 0)) {
    return;
  }

  auto iter = abortCommandList.find(id);

  if (iter != abortCommandList.end()) {
    // Aborted command finished
    schedule(iter->second, getTick());

    abortCommandList.erase(iter);
  }
}

bool Arbitrator::checkShutdown() {
  bool done = true;

  if (dispatchedQueue.size() > 0) {
    // Only AEN can be in dispatched queue
    for (auto iter = dispatchedQueue.begin(); iter != dispatchedQueue.end();
         ++iter) {
      auto sqc = (SQContext *)iter.getValue();

      if (sqc->sqID != 0 || sqc->entry.dword0.opcode !=
                                (uint8_t)AdminCommand::AsyncEventRequest) {
        // Currently dispatched request is not AEN
        done = false;
      }
    }
  }

  return done;
}

void Arbitrator::finishShutdown() {
  // All pending requests are finished
  controller->controller->shutdownComplete();

  // Free requests
  for (auto iter = dispatchedQueue.begin(); iter != dispatchedQueue.end();
       ++iter) {
    auto sqc = (SQContext *)iter.getValue();

    delete sqc;
  }

  dispatchedQueue.clear();

  for (auto iter = requestQueue.begin(); iter != requestQueue.end(); ++iter) {
    auto sqc = (SQContext *)iter.getValue();

    delete sqc;
  }

  requestQueue.clear();

  // Remove all queues (including admin queues)
  for (uint16_t i = 0; i < sqSize; i++) {
    if (sqList[i]) {
      delete sqList[i];
      sqList[i] = nullptr;
    }
  }

  for (uint16_t i = 0; i < cqSize; i++) {
    if (cqList[i]) {
      delete cqList[i];
      cqList[i] = nullptr;
    }
  }

  shutdownReserved = false;
}

void Arbitrator::collect(uint64_t now) {
  bool handled = false;

  lastInvokedAt = now;

  if (UNLIKELY(!run)) {
    return;
  }

  if (UNLIKELY(shutdownReserved)) {
    // Terminating
    run = false;

    // Clear ALL!!
    requestQueue.clear();

    // No pending requests && no pending completions
    if (checkShutdown() && !isScheduled(eventCompDone)) {
      // We can shutdown now
      finishShutdown();
    }

    return;
  }

  if (LIKELY(!running && requestQueue.size() < internalQueueSize)) {
    running = true;

    // Collect requests
    if (LIKELY(mode == Arbitration::RoundRobin)) {
      handled = collectRoundRobin();
    }
    else if (mode == Arbitration::WeightedRoundRobin) {
      handled = collectWeightedRoundRobin();
    }
    else {
      panic("Invalid arbitration mode");
    }
  }

  // Schedule collect
  lastInvokedAt += period;

  schedule(work, lastInvokedAt);

  if (!handled) {
    running = false;
  }
}

void Arbitrator::collect_done() {
  auto sqe = collectQueue.front();

  sqe->update();

  auto ret = requestQueue.push_back(sqe->getID(), sqe);

  collectQueue.pop_front();

  if (UNLIKELY(!ret)) {
    // Command ID duplication
    abortCommand(sqe, StatusType::GenericCommandStatus,
                 GenericCommandStatusCode::CommandIDConflict);

    delete sqe;
  }

  if (collectQueue.size() == 0) {
    running = false;

    controller->controller->notifySubsystem(internalQueueSize);
  }
}

bool Arbitrator::checkQueue(uint16_t qid) {
  auto sq = sqList[qid];

  if (sq && sq->getItemCount() > 0) {
    auto *entry = new SQContext();

    collectQueue.push_back(entry);

    entry->update(qid, sq->getCQID(), sq->getHead());
    sq->getData(entry->getData(), eventCollect);

    return true;
  }

  return false;
}

bool Arbitrator::collectRoundRobin() {
  bool ret = false;

  for (uint16_t i = 0; i < sqSize; i++) {
    ret |= checkQueue(i);
  }

  return ret;
}

bool Arbitrator::collectWeightedRoundRobin() {
  bool ret = false;
  uint16_t count;

  // Check urgent queues
  for (uint16_t i = 0; i < sqSize; i++) {
    if (sqList[i] && sqList[i]->getPriority() == QueuePriority::Urgent) {
      ret |= checkQueue(i);
    }
  }

  // Check high-priority queues
  count = 0;

  for (uint16_t i = 0; i < sqSize; i++) {
    if (sqList[i] && sqList[i]->getPriority() == QueuePriority::High) {
      if (ret |= checkQueue(i)) {
        count++;

        if (count > param.hpw) {  // Zero-based value
          break;
        }
      }
    }
  }

  // Check medium-priority queues
  count = 0;

  for (uint16_t i = 0; i < sqSize; i++) {
    if (sqList[i] && sqList[i]->getPriority() == QueuePriority::Medium) {
      if (ret |= checkQueue(i)) {
        count++;

        if (count > param.mpw) {  // Zero-based value
          break;
        }
      }
    }
  }

  // Check low-priority queues
  count = 0;

  for (uint16_t i = 0; i < sqSize; i++) {
    if (sqList[i] && sqList[i]->getPriority() == QueuePriority::Low) {
      if (ret |= checkQueue(i)) {
        count++;

        if (count > param.lpw) {  // Zero-based value
          break;
        }
      }
    }
  }

  return ret;
}

void Arbitrator::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Arbitrator::getStatValues(std::vector<double> &) noexcept {}

void Arbitrator::resetStatValues() noexcept {}

void Arbitrator::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, period);
  BACKUP_SCALAR(out, internalQueueSize);
  BACKUP_SCALAR(out, lastInvokedAt);
  BACKUP_SCALAR(out, cqSize);
  BACKUP_SCALAR(out, sqSize);
  BACKUP_SCALAR(out, mode);
  BACKUP_SCALAR(out, param);
  BACKUP_SCALAR(out, shutdownReserved);
  BACKUP_SCALAR(out, run);
  BACKUP_SCALAR(out, running);

  // We stored cqSize and sqSize
  bool tmp;

  for (uint16_t i = 0; i < cqSize; i++) {
    tmp = cqList[i] != nullptr;

    BACKUP_SCALAR(out, tmp);

    if (tmp) {
      cqList[i]->createCheckpoint(out);
    }
  }

  for (uint16_t i = 0; i < sqSize; i++) {
    tmp = sqList[i] != nullptr;

    BACKUP_SCALAR(out, tmp);

    if (tmp) {
      sqList[i]->createCheckpoint(out);
    }
  }

  // Store list by poping all entries
  auto size = requestQueue.size();
  BACKUP_SCALAR(out, size);

  for (auto iter = requestQueue.begin(); iter != requestQueue.end(); ++iter) {
    auto entry = (SQContext *)iter.getValue();

    BACKUP_BLOB(out, entry->entry.data, 64);
    BACKUP_SCALAR(out, entry->commandID);
    BACKUP_SCALAR(out, entry->sqID);
    BACKUP_SCALAR(out, entry->cqID);
    BACKUP_SCALAR(out, entry->sqHead);
    BACKUP_SCALAR(out, entry->useSGL);
    BACKUP_SCALAR(out, entry->dispatched);
    BACKUP_SCALAR(out, entry->completed);
  }

  size = dispatchedQueue.size();
  BACKUP_SCALAR(out, size);

  for (auto iter = dispatchedQueue.begin(); iter != dispatchedQueue.end();
       ++iter) {
    auto entry = (SQContext *)iter.getValue();

    BACKUP_BLOB(out, entry->entry.data, 64);
    BACKUP_SCALAR(out, entry->commandID);
    BACKUP_SCALAR(out, entry->sqID);
    BACKUP_SCALAR(out, entry->cqID);
    BACKUP_SCALAR(out, entry->sqHead);
    BACKUP_SCALAR(out, entry->useSGL);
    BACKUP_SCALAR(out, entry->dispatched);
    BACKUP_SCALAR(out, entry->completed);
  }

  size = completionQueue.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : completionQueue) {
    BACKUP_BLOB(out, iter->entry.data, 16);
    BACKUP_SCALAR(out, iter->cqID);
  }

  size = collectQueue.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : collectQueue) {
    // For collecting queue, only data is used
    BACKUP_BLOB(out, iter->entry.data, 64);
  }

  size = abortSQList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : abortSQList) {
    BACKUP_SCALAR(out, iter.first);
    BACKUP_SCALAR(out, iter.second);
  }

  size = abortCommandList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : abortCommandList) {
    BACKUP_SCALAR(out, iter.first);
    BACKUP_SCALAR(out, iter.second);
  }
}

void Arbitrator::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, period);
  RESTORE_SCALAR(in, internalQueueSize);
  RESTORE_SCALAR(in, lastInvokedAt);
  RESTORE_SCALAR(in, cqSize);
  RESTORE_SCALAR(in, sqSize);
  RESTORE_SCALAR(in, mode);
  RESTORE_SCALAR(in, param);
  RESTORE_SCALAR(in, shutdownReserved);
  RESTORE_SCALAR(in, run);
  RESTORE_SCALAR(in, running);

  // Restore queue
  // As we are using exactly same configuration, cqSize and sqSize will exactly
  // same with restore version.
  bool tmp;

  for (uint16_t i = 0; i < cqSize; i++) {
    RESTORE_SCALAR(in, tmp);

    if (tmp) {
      cqList[i]->restoreCheckpoint(in);
    }
    else {
      cqList[i] = nullptr;
    }
  }

  for (uint16_t i = 0; i < sqSize; i++) {
    RESTORE_SCALAR(in, tmp);

    if (tmp) {
      sqList[i]->restoreCheckpoint(in);
    }
    else {
      sqList[i] = nullptr;
    }
  }

  // Restore list by pushing all entries
  uint64_t size;
  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    auto iter = new SQContext();

    RESTORE_BLOB(in, iter->entry.data, 64);
    RESTORE_SCALAR(in, iter->commandID);
    RESTORE_SCALAR(in, iter->sqID);
    RESTORE_SCALAR(in, iter->cqID);
    RESTORE_SCALAR(in, iter->sqHead);
    RESTORE_SCALAR(in, iter->useSGL);
    RESTORE_SCALAR(in, iter->dispatched);
    RESTORE_SCALAR(in, iter->completed);

    requestQueue.push_back(iter->commandID, iter);
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    auto iter = new SQContext();

    RESTORE_BLOB(in, iter->entry.data, 64);
    RESTORE_SCALAR(in, iter->commandID);
    RESTORE_SCALAR(in, iter->sqID);
    RESTORE_SCALAR(in, iter->cqID);
    RESTORE_SCALAR(in, iter->sqHead);
    RESTORE_SCALAR(in, iter->useSGL);
    RESTORE_SCALAR(in, iter->dispatched);
    RESTORE_SCALAR(in, iter->completed);

    dispatchedQueue.push_back(iter->commandID, iter);
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    auto iter = new CQContext();

    RESTORE_BLOB(in, iter->entry.data, 16);
    RESTORE_SCALAR(in, iter->cqID);

    completionQueue.push_back(iter);
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    auto iter = new SQContext();

    RESTORE_BLOB(in, iter->entry.data, 64);

    collectQueue.push_back(iter);
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    uint16_t id;
    Event eid;

    RESTORE_SCALAR(in, id);
    RESTORE_SCALAR(in, eid);

    abortSQList.emplace(std::make_pair(id, eid));
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    uint32_t id;
    Event eid;

    RESTORE_SCALAR(in, id);
    RESTORE_SCALAR(in, eid);

    abortCommandList.emplace(std::make_pair(id, eid));
  }
}

SQContext *Arbitrator::getRecoveredRequest(uint32_t id) {
  // Query from dispatchedQueue
  return (SQContext *)dispatchedQueue.find(id);
}

}  // namespace SimpleSSD::HIL::NVMe
