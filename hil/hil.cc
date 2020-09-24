// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/hil.hh"

namespace SimpleSSD::HIL {

HIL::HIL(ObjectData &o, AbstractSubsystem *p)
    : Object(o),
      parent(p),
      icl(object, this),
      requestCounter(0),
      subrequestCounter(0) {
  lpnSize = icl.getLPNSize();

  Convert convert(object, lpnSize);

  convertFunction = convert.getConvertion();

  // Create events
  eventNVMCompletion =
      createEvent([this](uint64_t t, uint64_t d) { nvmCompletion(t, d); },
                  "HIL::HIL::eventNVMCompletion");
  eventDMACompletion =
      createEvent([this](uint64_t t, uint64_t d) { dmaCompletion(t, d); },
                  "HIL::HIL::eventDMACompletion");

  // Register callback to ICL layer
  icl.setCallbackFunction(eventNVMCompletion);
}

HIL::~HIL() {
  warn_if(requestQueue.size() != 0,
          "Not all requests are handled (%" PRIu64 " left).",
          requestQueue.size());
  warn_if(subrequestQueue.size() != 0,
          "Not all subrequests are handled (%" PRIu64 " left).",
          subrequestQueue.size());
}

void HIL::submit(Operation opcode, Request *req) {
  uint64_t tag = ++requestCounter;

  req->opcode = opcode;
  req->requestTag = tag;

  auto ret = requestQueue.push_back(tag, req);

  panic_if(!ret.second, "Request ID conflict.");

  // Make LPN address
  LPN slpn;
  uint32_t nlp;
  uint32_t skipFront;
  uint32_t skipEnd;

  convertFunction(req->offset, req->length, slpn, nlp, skipFront, skipEnd);

  panic_if(nlp == 0, "Unexpected length of request.");

  if (LIKELY(req->eid != InvalidEventID)) {
    uint64_t _uid = req->hostTag;

    debugprint(Log::DebugID::HIL,
               "%s | %3u:%u:%-5u -> %7" PRIu64 " | LPN %" PRIu64 " + %" PRIu64
               " | BYTE %" PRIu64 " + %" PRIu64,
               getOperationName(req->opcode), HIGH32(_uid), HIGH16(_uid),
               LOW16(_uid), tag, slpn, nlp, skipFront, req->length);
  }
  else {
    // Prefetch/readahead
    debugprint(Log::DebugID::HIL,
               "%s | FROM ICL | REQ %7" PRIu64 " | LPN %" PRIu64 " + %" PRIu64
               " | BYTE %" PRIu64 " + %" PRIu64,
               getOperationName(req->opcode), tag, slpn, nlp, skipFront,
               req->length);
  }

  if (opcode < Operation::Flush) {
    // Make subrequests
    for (uint32_t i = 0; i < nlp; i++) {
      uint64_t offset = lpnSize * i - skipFront;
      uint32_t length = lpnSize;
      uint32_t sf = 0;
      uint32_t se = 0;

      if (i == 0) {
        offset = 0;
        length -= skipFront;
        sf = skipFront;
      }
      if (i + 1 == nlp) {
        length -= skipEnd;
        se = skipEnd;
      }

      // Insert
      ++subrequestCounter;

      auto sreq = subrequestQueue.emplace(
          subrequestCounter,
          SubRequest(subrequestCounter, req, static_cast<LPN>(slpn + i), offset,
                     length));

      panic_if(!sreq.second, "SubRequest ID conflict.");

      sreq.first->second.skipFront = sf;
      sreq.first->second.skipEnd = se;
    }

    req->slpn = slpn;
    req->nlp = nlp;
  }
  else {
    ++subrequestCounter;

    auto sreq = subrequestQueue.emplace(subrequestCounter,
                                        SubRequest(subrequestCounter, req));

    panic_if(!sreq.second, "SubRequest ID conflict.");

    sreq.first->second.offset = slpn;
    sreq.first->second.length = nlp;

    req->nlp = 1;
  }

  req->firstSubRequestTag = subrequestCounter - req->nlp + 1;
}

void HIL::dispatch(Request *req) {
  uint64_t stagBegin = req->firstSubRequestTag;
  uint64_t stagEnd = req->firstSubRequestTag + req->nlp;

  std::vector<SubRequest *> subrequestList;

  // Make subrequestList
  subrequestList.reserve(req->nlp);

  for (uint64_t t = stagBegin; t < stagEnd; t++) {
    auto siter = subrequestQueue.find(t);

    panic_if(siter == subrequestQueue.end(), "Unexpected SubRequest ID.");

    subrequestList.emplace_back(&siter->second);
  }

  req->nvmBeginAt = getTick();

  switch (req->opcode) {
    case Operation::Compare:
    case Operation::CompareAndWrite:
      /*
       * For compare, we can perform DMA and NVM operation in parallel. Trigger
       * all NVM and DMA operation.
       */

      panic("Compare not implemented yet.");

      [[fallthrough]];
    case Operation::Read:
      /*
       * For read, we push all NVM requests at same time, and do the DMA when
       * each subrequest is completed.
       */
      for (auto &sreq : subrequestList) {
        icl.read(sreq);
      }

      break;
    case Operation::Write:
      /*
       * For write, we need to read data to be written by DMA operation. But
       * before starting DMA, we need to know where to copy data. So, we first
       * handle cache lookup to get target DRAM address to copy data. Second,
       * we perform DMA.
       */
    case Operation::WriteZeroes:
      /*
       * For write zeroes, we don't need to perform DMA. Just push all NVM
       * requests at same time, like read operation.
       */
      for (auto &sreq : subrequestList) {
        icl.write(sreq);
      }

      break;
    case Operation::Flush:
      for (auto &sreq : subrequestList) {
        icl.flush(sreq);
      }

      break;
    case Operation::Trim:
    case Operation::Format:
      for (auto &sreq : subrequestList) {
        icl.format(sreq);
      }

      break;
    default:
      panic("Unexpected opcode.");

      break;
  }
}

void HIL::nvmCompletion(uint64_t now, uint64_t tag) {
  bool remove = false;
  auto iter = subrequestQueue.find(tag);

  panic_if(iter == subrequestQueue.end(), "Unexpected subrequest %" PRIx64 "h.",
           tag);

  // Shortcuts
  auto &sreq = iter->second;
  auto &req = *iter->second.request;

  // Increase counter
  req.nvmCounter++;

  panic_if(req.nvmCounter > req.nlp, "I/O event corrupted.");

  // Make buffer pointer
  uint8_t *buffer = req.buffer ? req.buffer + sreq.offset : nullptr;

  switch (req.opcode) {
    case Operation::Read:
      if (req.eid == InvalidEventID) {
        // This is prefetch request
        remove = true;

        // Remove request
        if (req.nvmCounter == req.nlp) {
          auto iter = requestQueue.find(req.requestTag);

          requestQueue.erase(iter);

          delete &req;
        }
      }
      else {
        // Submit DMA (current chunk)
        if (req.dmaCounter == 0) {
          req.dmaBeginAt = now;
        }

        if (LIKELY(req.dmaEngine)) {
          req.dmaEngine->write(req.dmaTag, sreq.offset, sreq.length, buffer,
                               sreq.address + sreq.skipFront,
                               eventDMACompletion, sreq.requestTag);
        }
        else {
          // No DMA Engine -- None interface
          object.memory->read(sreq.address + sreq.skipFront, sreq.length,
                              eventDMACompletion, sreq.requestTag, false);
        }
      }

      break;
    case Operation::Write:
      // Submit DMA (current chunk)
      if (req.dmaCounter == 0) {
        req.dmaBeginAt = now;
      }

      if (LIKELY(req.dmaEngine)) {
        req.dmaEngine->read(req.dmaTag, sreq.offset, sreq.length, buffer,
                            sreq.address + sreq.skipFront, eventDMACompletion,
                            sreq.requestTag);
      }
      else {
        // No DMA Engine -- None interface
        object.memory->write(sreq.address + sreq.skipFront, sreq.length,
                             eventDMACompletion, sreq.requestTag, false);
      }

      break;
    case Operation::WriteZeroes:
      // Write-zeroes does not require DMA operation
      icl.done(&sreq);

      [[fallthrough]];
    case Operation::Flush:
    case Operation::Trim:
    case Operation::Format:
      // Complete when all pending NVM operations are completed.
      remove = true;

      if (req.nvmCounter == req.nlp) {
        scheduleNow(req.eid, req.data);

        // Remove request
        auto iter = requestQueue.find(req.requestTag);

        requestQueue.erase(iter);
      }

      break;
    case Operation::Compare:
    case Operation::CompareAndWrite:
      // Check DMA operations are completed, and start compare operation
      panic("Compare not implemented yet.");

      break;
    default:
      panic("Unexpected opcode in NVM completion.");

      break;
  }

  if (remove) {
    subrequestQueue.erase(iter);
  }
}

void HIL::dmaCompletion(uint64_t now, uint64_t tag) {
  bool remove = false;
  auto iter = subrequestQueue.find(tag);

  panic_if(iter == subrequestQueue.end(), "Unexpected subrequest %" PRIx64 "h",
           tag);

  // Shortcuts
  auto &sreq = iter->second;
  auto &req = *iter->second.request;

  // Increase counter
  req.dmaCounter++;

  panic_if(req.dmaCounter > req.nlp, "DMA event corrupted.");

  switch (req.opcode) {
    case Operation::Read:
    case Operation::Write:
      // Complete when all pending DMA operations are completed.
      remove = true;

      icl.done(&sreq);  // Mark as complete

      if (req.dmaCounter == req.nlp) {
        // Apply stat
        auto &stat = req.opcode == Operation::Read ? readStat : writeStat;

        stat.add(req.nlp * lpnSize, now - req.nvmBeginAt);

        // Invoke callback
        scheduleAbs(req.eid, req.data, now);

        // Remove request
        auto iter = requestQueue.find(req.requestTag);

        requestQueue.erase(iter);
      }

      break;
    case Operation::Compare:
    case Operation::CompareAndWrite:
      // Check NVM operations are completed, and start compare operation
      panic("Compare not implemented yet.");

      break;
    default:
      panic("Unexpected opcode in DMA completion.");

      break;
  }

  if (remove) {
    subrequestQueue.erase(iter);
  }
}

void HIL::read(Request *req) {
  submit(Operation::Read, req);

  if (UNLIKELY(req->eid == InvalidEventID)) {
    // Immediate dispatch
    dispatch(req);
  }
}

void HIL::write(Request *req, bool zerofill) {
  submit(zerofill ? Operation::WriteZeroes : Operation::Write, req);
}

void HIL::flush(Request *req) {
  submit(Operation::Flush, req);

  // Immediate dispatch
  dispatch(req);
}

void HIL::format(Request *req, FormatOption option) {
  if (option == FormatOption::CryptographicErase) {
    warn("Cryptographic erase not implemented.");

    option = FormatOption::UserDataErase;
  }

  submit(option == FormatOption::None ? Operation::Trim : Operation::Format,
         req);

  // Immediate dispatch
  dispatch(req);
}

void HIL::compare(Request *req, bool fused) {
  if (fused) {
    // Lazy submit until corresponding write is submitted
    panic("Fused operation not supported yet.");
  }
  else {
    submit(Operation::Compare, req);
  }
}

void HIL::notifyDMAInited(uint64_t tag) {
  auto iter = requestQueue.find(tag);

  panic_if(iter == requestQueue.end(), "Unexpected Request ID.");
  panic_if(!iter->second->dmaTag->isInited(), "DMA Not inited?");

  if (iter != requestQueue.begin()) {
    auto copy = iter;

    --copy;

    if (copy->second->nvmBeginAt == 0) {
      // Not first item, skip dispatch
      return;
    }
  }

  do {
    dispatch(iter->second);

    ++iter;
  } while (iter != requestQueue.end() && iter->second->dmaTag->isInited());
}

uint64_t HIL::getPageUsage(LPN offset, uint64_t length) {
  return icl.getPageUsage(offset, length);
}

uint64_t HIL::getTotalPages() {
  return icl.getTotalPages();
}

uint32_t HIL::getLPNSize() {
  return icl.getLPNSize();
}

SubRequest *HIL::getSubRequest(uint64_t tag) {
  auto iter = subrequestQueue.find(tag);

  panic_if(iter == subrequestQueue.end(), "Unexpected SubRequest %" PRIu64 ".",
           tag);

  return &iter->second;
}

void HIL::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  list.emplace_back(prefix + "hil.read.count", "Total read requests");
  list.emplace_back(prefix + "hil.read.pages", "Total read pages");
  list.emplace_back(prefix + "hil.read.latency.average",
                    "Average read latency");
  list.emplace_back(prefix + "hil.read.latency.min", "Minimum read latency");
  list.emplace_back(prefix + "hil.read.latency.max", "Maximum read latency");
  list.emplace_back(prefix + "hil.write.count", "Total write requests");
  list.emplace_back(prefix + "hil.write.pages", "Total write pages");
  list.emplace_back(prefix + "hil.write.latency.average",
                    "Average write latency");
  list.emplace_back(prefix + "hil.write.latency.min", "Minimum write latency");
  list.emplace_back(prefix + "hil.write.latency.max", "Maximum write latency");

  icl.getStatList(list, prefix);
}

void HIL::getStatValues(std::vector<double> &values) noexcept {
  values.push_back((double)readStat.getCount());
  values.push_back((double)readStat.getSize() / lpnSize);
  values.push_back((double)readStat.getAverageLatency());
  values.push_back((double)readStat.getMinimumLatency());
  values.push_back((double)readStat.getMaximumLatency());
  values.push_back((double)writeStat.getCount());
  values.push_back((double)writeStat.getSize() / lpnSize);
  values.push_back((double)writeStat.getAverageLatency());
  values.push_back((double)writeStat.getMinimumLatency());
  values.push_back((double)writeStat.getMaximumLatency());

  icl.getStatValues(values);
}

void HIL::resetStatValues() noexcept {
  readStat.clear();
  writeStat.clear();

  icl.resetStatValues();
}

void HIL::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, requestCounter);
  BACKUP_SCALAR(out, subrequestCounter);
  BACKUP_EVENT(out, eventNVMCompletion);
  BACKUP_EVENT(out, eventDMACompletion);

  uint64_t size = requestQueue.size();

  BACKUP_SCALAR(out, size);

  for (auto &iter : requestQueue) {
    BACKUP_SCALAR(out, iter.first);
  }

  size = subrequestQueue.size();

  BACKUP_SCALAR(out, size);

  for (auto &iter : subrequestQueue) {
    BACKUP_SCALAR(out, iter.first);

    iter.second.createCheckpoint(out);
  }

  readStat.createCheckpoint(out);
  writeStat.createCheckpoint(out);

  icl.createCheckpoint(out);
}

void HIL::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, requestCounter);
  RESTORE_SCALAR(in, subrequestCounter);
  RESTORE_EVENT(in, eventNVMCompletion);
  RESTORE_EVENT(in, eventDMACompletion);

  uint64_t size, tag;
  Request *req;

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    RESTORE_SCALAR(in, tag);

    req = parent->restoreRequest(tag);

    panic_if(req == nullptr, "Invalid request while restore.");

    requestQueue.push_back(tag, req);
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    RESTORE_SCALAR(in, tag);

    SubRequest sreq;

    sreq.restoreCheckpoint(in, this);
  }

  readStat.restoreCheckpoint(in);
  writeStat.restoreCheckpoint(in);

  icl.restoreCheckpoint(in);
}

Request *HIL::restoreRequest(uint64_t tag) noexcept {
  auto iter = requestQueue.find(tag);

  panic_if(iter == requestQueue.end(), "Invalid request tag while restore.");

  return iter->second;
}

SubRequest *HIL::restoreSubRequest(uint64_t tag) noexcept {
  auto iter = subrequestQueue.find(tag);

  panic_if(iter == subrequestQueue.end(),
           "Invalid subrequest tag while restore.");

  return &iter->second;
}

}  // namespace SimpleSSD::HIL
