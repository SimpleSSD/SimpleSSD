// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/queue_arbitrator.hh"

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

void CQContext::makeStatus(bool dnr, bool more, StatusType sct,
                           uint8_t sc) noexcept {
  entry.dword3.status = 0;  // Phase field will be filled before DMA

  entry.dword3.dnr = dnr ? 1 : 0;
  entry.dword3.more = more ? 1 : 0;
  entry.dword3.sct = (uint8_t)sct;
  entry.dword3.sc = sc;
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

Arbitrator::Arbitrator(ObjectData &o)
    : Object(o),
      inited(false),
      submit(InvalidEventID),
      interrupt(InvalidEventID),
      mode(Arbitration::RoundRobin) {
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
  complete = createEvent(
      [this](uint64_t t, void *c) { completion(t, (CQContext *)c); },
      "HIL::NVMe::Arbitrator::complete");
  work = createEvent([this](uint64_t t, void *) { collect(t); },
                     "HIL::NVMe::Arbitrator::work");
  eventCompDone = createEvent(
      [this](uint64_t t, void *c) { completion_done(t, (CQContext *)c); },
      "HIL::NVMe::Arbitrator::eventCompDone");
  eventCollect = createEvent(
      [this](uint64_t t, void *c) { collect_done(t, (SQContext *)c); },
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

Event Arbitrator::init(Event s, Event i) {
  submit = s;
  interrupt = i;

  inited = true;

  return complete;
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
    intrruptContext.iv = cq->getInterruptVector();
    intrruptContext.post = false;

    schedule(interrupt, getTick(), &intrruptContext);
  }
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

SQContext *Arbitrator::dispatch() {
  auto entry = (SQContext *)requestQueue.front();

  entry->dispatched = true;

  dispatchedQueue.push_back(entry->commandID, entry);
  requestQueue.pop_front();

  return entry;
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

void Arbitrator::completion(uint64_t, CQContext *cqe) {
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

  // Write entry to CQ
  cq->setData(cqe->getData(), eventCompDone, cqe);
}

void Arbitrator::completion_done(uint64_t now, CQContext *cqe) {
  auto cq = cqList[cqe->getCQID()];

  intrruptContext.iv = cq->getInterruptVector();
  intrruptContext.post = true;

  schedule(interrupt, now, &intrruptContext);

  // Remove CQContext
  delete cqe;
}

void Arbitrator::collect(uint64_t now) {
  if (UNLIKELY(!run || running)) {
    return;
  }

  if (requestQueue.size() >= internalQueueSize) {
    // Nothing to do!
    schedule(work, now + period);

    return;
  }

  running = true;

  // Collect requests
  collectRequested = 0;
  collectCompleted = 0;

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

void Arbitrator::collect_done(uint64_t now, SQContext *sqe) {
  collectCompleted++;

  sqe->update();

  requestQueue.push_back(sqe->getCommandID(), sqe);

  if (collectCompleted == collectRequested) {
    running = false;

    schedule(submit, now, this);
  }
}

bool Arbitrator::checkQueue(uint16_t qid) {
  auto sq = sqList[qid];

  if (sq && sq->getItemCount() > 0) {
    SQContext *entry = new SQContext();

    collectRequested++;

    entry->update(qid, sq->getCQID(), sq->getHead());
    sq->getData(entry->getData(), eventCollect, entry);

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

}  // namespace SimpleSSD::HIL::NVMe
