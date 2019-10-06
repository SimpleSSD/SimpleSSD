// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/queue_arbitrator.hh"

#include "hil/nvme/controller.hh"

namespace SimpleSSD::HIL::NVMe {

SQContext::SQContext()
    : commandID(0),
      sqID(0),
      cqID(0),
      sqHead(0),
      useSGL(false),
      aborted(false),
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

uint16_t SQContext::getCommandID() noexcept {
  return commandID;
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

bool SQContext::isAbort() noexcept {
  return aborted;
}

void SQContext::abort() noexcept {
  aborted = true;
}

CQContext::CQContext() : cqID(0) {}

void CQContext::update(SQContext *sqe) noexcept {
  cqID = sqe->cqID;
  entry.dword2.sqHead = sqe->sqHead;
  entry.dword2.sqID = sqe->sqID;
  entry.dword3.commandID = sqe->entry.dword0.commandID;

  sqe->completed = true;
}

template <
    class Type,
    std::enable_if_t<
        std::conjunction_v<std::is_enum<Type>,
                           std::is_same<std::underlying_type_t<Type>, uint8_t>>,
        Type>
        T>
void CQContext::makeStatus(bool dnr, bool more, StatusType sct,
                           Type sc) noexcept {
  entry.dword3.status = 0;  // Phase field will be filled before DMA

  entry.dword3.dnr = dnr ? 1 : 0;
  entry.dword3.more = more ? 1 : 0;
  entry.dword3.sct = (uint8_t)sct;
  entry.dword3.sc = (uint8_t)sc;
}

bool CQContext::isSuccess() noexcept {
  return entry.dword3.sct == (uint8_t)StatusType::GenericCommandStatus &&
         entry.dword3.sc == (uint8_t)GenericCommandStatusCode::Success;
}

CQEntry *CQContext::getData() {
  return &entry;
}

uint16_t CQContext::getCommandID() noexcept {
  return entry.dword3.commandID;
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
  eventCompDone = createEvent([this](uint64_t t) { completion_done(t); },
                              "HIL::NVMe::Arbitrator::eventCompDone");
  eventCollect = createEvent([this](uint64_t t) { collect_done(t); },
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

  sq->setTail(tail);
}

void Arbitrator::ringCQ(uint16_t qid, uint16_t head) {
  auto cq = cqList[qid];

  panic_if(!cq, "Access to unintialized completion queue.");

  cq->setHead(head);

  if (cq->getItemCount() == 0) {
    controller->interruptManager->postInterrupt(cq->getInterruptVector(),
                                                false);
  }
}

SQContext *Arbitrator::dispatch() {
  auto entry = (SQContext *)requestQueue.front();

  entry->dispatched = true;

  dispatchedQueue.push_back(entry->commandID, entry);
  requestQueue.pop_front();

  return entry;
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
}

// bool Arbitrator::submitCommand(SQContext *sqe) {
//   switch ((AdminCommand)sqe->entry.dword0.opcode) {
//     case AdminCommand::DeleteIOSQ:
//       return deleteSQ(sqe);
//     case AdminCommand::CreateIOSQ:
//       return createSQ(sqe);
//     case AdminCommand::DeleteIOCQ:
//       return deleteCQ(sqe);
//     case AdminCommand::CreateIOCQ:
//       return createCQ(sqe);
//     case AdminCommand::Abort:
//       return abort(sqe);
//     case AdminCommand::SetFeatures:
//       return setFeature(sqe);
//     case AdminCommand::GetFeatures:
//       return getFeature(sqe);
//   }

//   return false;
// }

void Arbitrator::complete(CQContext *cqe) {
  auto cq = cqList[cqe->getCQID()];

  panic_if(!cq, "Completion to invalid completion queue.");

  // Find corresponding SQContext
  auto sqe = (SQContext *)dispatchedQueue.find(cqe->getCommandID());

  panic_if(!sqe, "Failed to find corresponding submission entry.");
  panic_if(!sqe->completed, "Corresponding submission entry not completed.");

  if (UNLIKELY(
          sqe->aborted &&
          cqe->entry.dword3.sct == (uint8_t)StatusType::GenericCommandStatus &&
          cqe->entry.dword3.sc == (uint8_t)GenericCommandStatusCode::Success)) {
    // Aborted after completion
    // TODO: FILL HERE
  }

  // Remove SQContext
  dispatchedQueue.erase(sqe->commandID);
  delete sqe;

  // Insert to completion queue
  completionQueue.push(cqe);

  // Write entry to CQ
  cq->setData(cqe->getData(), eventCompDone);

  if (UNLIKELY(shutdownReserved && dispatchedQueue.size() == 0)) {
    // All pending requests are finished
    controller->controller->shutdownComplete();

    shutdownReserved = false;
  }
}

void Arbitrator::completion_done(uint64_t now) {
  auto cqe = completionQueue.front();
  auto cq = cqList[cqe->getCQID()];

  completionQueue.pop();

  controller->interruptManager->postInterrupt(cq->getInterruptVector(), true);

  // Remove CQContext
  delete cqe;
}

void Arbitrator::collect(uint64_t now) {
  if (UNLIKELY(!run || running)) {
    return;
  }

  if (UNLIKELY(shutdownReserved)) {
    // Terminating
    run = false;

    // Clear ALL!!
    requestQueue.clear();

    return;
  }

  if (requestQueue.size() >= internalQueueSize) {
    // Nothing to do!
    schedule(work, now + period);

    return;
  }

  running = true;

  // Collect requests
  if (LIKELY(mode == Arbitration::RoundRobin)) {
    collectRoundRobin();
  }
  else if (mode == Arbitration::WeightedRoundRobin) {
    collectWeightedRoundRobin();
  }
  else {
    panic("Invalid arbitration mode");
  }
}

void Arbitrator::collect_done(uint64_t now) {
  auto sqe = collectQueue.front();

  sqe->update();

  requestQueue.push_back(sqe->getCommandID(), sqe);

  collectQueue.pop();

  if (collectQueue.size() == 0) {
    running = false;

    controller->controller->notifySubsystem();
  }
}

bool Arbitrator::checkQueue(uint16_t qid) {
  auto sq = sqList[qid];

  if (sq && sq->getItemCount() > 0) {
    auto *entry = new SQContext();

    collectQueue.push(entry);

    entry->update(qid, sq->getCQID(), sq->getHead());
    sq->getData(entry->getData(), eventCollect);

    return true;
  }

  return false;
}

void Arbitrator::collectRoundRobin() {
  for (uint16_t i = 0; i < sqSize; i++) {
    checkQueue(i);
  }
}

void Arbitrator::collectWeightedRoundRobin() {
  uint16_t count;

  // Check urgent queues
  for (uint16_t i = 0; i < sqSize; i++) {
    if (sqList[i] && sqList[i]->getPriority() == QueuePriority::Urgent) {
      checkQueue(i);
    }
  }

  // Check high-priority queues
  count = 0;

  for (uint16_t i = 0; i < sqSize; i++) {
    if (sqList[i] && sqList[i]->getPriority() == QueuePriority::High) {
      if (checkQueue(i)) {
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
      if (checkQueue(i)) {
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
      if (checkQueue(i)) {
        count++;

        if (count > param.lpw) {  // Zero-based value
          break;
        }
      }
    }
  }
}

void Arbitrator::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Arbitrator::getStatValues(std::vector<double> &) noexcept {}

void Arbitrator::resetStatValues() noexcept {}

void Arbitrator::createCheckpoint(std::ostream &out) noexcept {
  BACKUP_SCALAR(out, period);
  BACKUP_SCALAR(out, internalQueueSize);
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

  while (auto iter = (SQContext *)requestQueue.front()) {
    BACKUP_BLOB(out, iter->entry.data, 64);
    BACKUP_SCALAR(out, iter->commandID);
    BACKUP_SCALAR(out, iter->sqID);
    BACKUP_SCALAR(out, iter->cqID);
    BACKUP_SCALAR(out, iter->sqHead);
    BACKUP_SCALAR(out, iter->useSGL);
    BACKUP_SCALAR(out, iter->aborted);
    BACKUP_SCALAR(out, iter->dispatched);
    BACKUP_SCALAR(out, iter->completed);

    requestQueue.erase(iter->commandID);
    delete iter;
  }

  size = dispatchedQueue.size();
  BACKUP_SCALAR(out, size);

  while (auto iter = (SQContext *)dispatchedQueue.front()) {
    BACKUP_BLOB(out, iter->entry.data, 64);
    BACKUP_SCALAR(out, iter->commandID);
    BACKUP_SCALAR(out, iter->sqID);
    BACKUP_SCALAR(out, iter->cqID);
    BACKUP_SCALAR(out, iter->sqHead);
    BACKUP_SCALAR(out, iter->useSGL);
    BACKUP_SCALAR(out, iter->aborted);
    BACKUP_SCALAR(out, iter->dispatched);
    BACKUP_SCALAR(out, iter->completed);

    dispatchedQueue.erase(iter->commandID);
    delete iter;
  }

  size = completionQueue.size();
  BACKUP_SCALAR(out, size);

  for (uint64_t i = 0; i < size; i++) {
    auto iter = completionQueue.front();

    BACKUP_BLOB(out, iter->entry.data, 16);
    BACKUP_SCALAR(out, iter->cqID);

    completionQueue.pop();
    delete iter;
  }

  size = collectQueue.size();
  BACKUP_SCALAR(out, size);

  for (uint64_t i = 0; i < size; i++) {
    auto iter = collectQueue.front();

    // For collecting queue, only data is used
    BACKUP_BLOB(out, iter->entry.data, 64);

    collectQueue.pop();
    delete iter;
  }
}

void Arbitrator::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, period);
  RESTORE_SCALAR(in, internalQueueSize);
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
    RESTORE_SCALAR(in, iter->aborted);
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
    RESTORE_SCALAR(in, iter->aborted);
    RESTORE_SCALAR(in, iter->dispatched);
    RESTORE_SCALAR(in, iter->completed);

    dispatchedQueue.push_back(iter->commandID, iter);
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    auto iter = new CQContext();

    RESTORE_BLOB(in, iter->entry.data, 16);
    RESTORE_SCALAR(in, iter->cqID);

    completionQueue.push(iter);
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    auto iter = new SQContext();

    RESTORE_BLOB(in, iter->entry.data, 64);

    collectQueue.push(iter);
  }
}

SQContext *Arbitrator::getRecoveredRequest(uint16_t cid) {
  // Query from dispatchedQueue
  return (SQContext *)dispatchedQueue.find(cid);
}

}  // namespace SimpleSSD::HIL::NVMe
