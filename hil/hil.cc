// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/hil.hh"

namespace SimpleSSD::HIL {

HIL::HIL(ObjectData &o)
    : Object(o), icl(object), requestCounter(0), subrequestCounter(0) {
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
  warn_if(requestQueue.size() != 0, "Not all requests are handled.");
  warn_if(subrequestQueue.size() != 0, "Not all subrequests are handled.");
}

void HIL::submit(Operation opcode, Request *req) {
  uint64_t tag = ++requestCounter;

  req->opcode = opcode;
  req->requestTag = tag;

  auto ret = requestQueue.emplace(tag, req);

  panic_if(!ret.second, "Request ID conflict.");

  std::vector<SubRequest *> subrequestList;

  if (opcode < Operation::Flush) {
    // Make LPN address
    LPN slpn;
    uint32_t nlp;
    uint32_t skipFront;
    uint32_t skipEnd;

    convertFunction(req->offset, req->length, slpn, nlp, skipFront, skipEnd);

    panic_if(req->nlp == 0, "Unexpected length of request.");

    // Make subrequests
    for (uint32_t i = 0; i < nlp; i++) {
      uint64_t offset = lpnSize * i - skipFront;
      uint32_t length = lpnSize;

      if (i == 0) {
        offset = 0;
        length -= skipFront;
      }
      if (i + 1 == nlp) {
        length -= skipEnd;
      }

      // Insert
      ++subrequestCounter;

      auto sreq = subrequestQueue.emplace(
          subrequestCounter,
          SubRequest(subrequestCounter, req, slpn + i, offset, length));

      panic_if(!sreq.second, "SubRequest ID conflict.");

      subrequestList.emplace_back(&sreq.first->second);
    }
  }
  else {
    ++subrequestCounter;

    auto sreq = subrequestQueue.emplace(subrequestCounter,
                                        SubRequest(subrequestCounter, req));

    panic_if(!sreq.second, "SubRequest ID conflict.");

    sreq.first->second.offset = req->offset;
    sreq.first->second.length = req->length;

    subrequestList.emplace_back(&sreq.first->second);
  }

  req->nlp = subrequestList.size();
  req->nvmBeginAt = getTick();

  auto &sreq = *subrequestList.front();

  switch (opcode) {
    case Operation::Compare:
    case Operation::CompareAndWrite:
      /*
       * For compare, we can perform DMA and NVM operation in parallel. Trigger
       * all NVM and DMA operation.
       */

      panic("Compare not implemented yet.");

      /* fallthrough */
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

  switch (req.opcode) {
    case Operation::Read:
      // Submit DMA (current chunk)
      if (req.dmaCounter == 0) {
        req.dmaBeginAt = now;
      }

      req.dmaEngine->write(req.dmaTag, sreq.offset, sreq.length, sreq.buffer,
                           eventDMACompletion, sreq.requestTag);

      break;
    case Operation::Write:
      // Submit DMA (current chunk)
      if (req.dmaCounter == 0) {
        req.dmaBeginAt = now;
      }

      req.dmaEngine->read(req.dmaTag, sreq.offset, sreq.length, sreq.buffer,
                          eventDMACompletion, sreq.requestTag);

      break;
    case Operation::WriteZeroes:
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
      // Complete when all pending DMA operations are completed.
      remove = true;

      icl.done(&sreq);  // Mark as complete

      if (req.dmaCounter == req.nlp) {
        scheduleNow(req.eid, req.data);

        // Remove request
        auto iter = requestQueue.find(req.requestTag);

        requestQueue.erase(iter);
      }

      break;
    case Operation::Write:
      // Complete when all pending DMA operations are completed.
      remove = true;

      icl.done(&sreq);  // Mark as complete

      if (req.dmaCounter == req.nlp) {
        scheduleNow(req.eid, req.data);

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
}

void HIL::write(Request *req, bool zerofill) {
  submit(zerofill ? Operation::WriteZeroes : Operation::WriteZeroes, req);
}

void HIL::flush(Request *req) {
  submit(Operation::Flush, req);
}

void HIL::format(Request *req, FormatOption option) {
  if (option == FormatOption::CryptographicErase) {
    warn("Cryptographic erase not implemented.");

    option = FormatOption::UserDataErase;
  }

  submit(option == FormatOption::None ? Operation::Trim : Operation::Format,
         req);
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

LPN HIL::getPageUsage(LPN offset, LPN length) {
  return icl.getPageUsage(offset, length);
}

LPN HIL::getTotalPages() {
  return icl.getTotalPages();
}

uint32_t HIL::getLPNSize() {
  return icl.getLPNSize();
}

void HIL::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  icl.getStatList(list, prefix);
}

void HIL::getStatValues(std::vector<double> &values) noexcept {
  icl.getStatValues(values);
}

void HIL::resetStatValues() noexcept {
  icl.resetStatValues();
}

void HIL::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, requestCounter);

  icl.createCheckpoint(out);
}

void HIL::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, requestCounter);

  icl.restoreCheckpoint(in);
}

}  // namespace SimpleSSD::HIL
