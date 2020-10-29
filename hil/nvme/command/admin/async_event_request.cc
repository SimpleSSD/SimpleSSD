// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

AsyncEventRequest::AsyncEventRequest(ObjectData &o, Subsystem *s)
    : Command(o, s) {}

void AsyncEventRequest::setRequest(ControllerData *cdata, AbstractNamespace *,
                                   SQContext *req) {
  auto tag = createTag(cdata, req);

  debugprint_command(tag, "ADMIN   | Asynchronous Event Request");

  // Do nothing
}

void AsyncEventRequest::invokeAEN(ControllerID ctrlid, AsyncEventType aet,
                                  uint8_t aei, LogPageID lid) {
  CommandTag tag = InvalidCommandTag;
  uint32_t aenData;

  aenData = (uint8_t)aet;
  aenData = (uint16_t)aei << 8;
  aenData = (uint32_t)lid << 16;

  // Get first AEN command of each controller
  for (auto &iter : tagList) {
    if ((iter.first >> 32) == ctrlid) {
      tag = iter.second;
    }
  }

  // Fill entry
  tag->cqc->getData()->dword0 = aenData;

  debugprint_command(tag, "Asynchronous Event | Type %u | Info %u | Log %u",
                     aet, aei, lid);

  // Complete
  subsystem->complete(tag);
}

void AsyncEventRequest::clearPendingRequests(ControllerID ctrlid) {
  for (auto iter = tagList.begin(); iter != tagList.end();) {
    if ((iter->first >> 32) == ctrlid) {
      delete iter->second;

      iter = tagList.erase(iter);
    }
    else {
      ++iter;
    }
  }
}

void AsyncEventRequest::getStatList(std::vector<Stat> &, std::string) noexcept {
}

void AsyncEventRequest::getStatValues(std::vector<double> &) noexcept {}

void AsyncEventRequest::resetStatValues() noexcept {}

}  // namespace SimpleSSD::HIL::NVMe
