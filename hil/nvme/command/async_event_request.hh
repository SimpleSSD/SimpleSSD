// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NVME_COMMAND_ASYNC_EVENT_REQUEST_HH__
#define __SIMPLESSD_HIL_NVME_COMMAND_ASYNC_EVENT_REQUEST_HH__

#include "hil/nvme/command/abstract_command.hh"

namespace SimpleSSD::HIL::NVMe {

class AsyncEventRequest : public Command {
 public:
  AsyncEventRequest(ObjectData &o, Subsystem *s, ControllerData *c)
      : Command(o, s, c) {}
  ~AsyncEventRequest() {}

  void setRequest(SQContext *req) override {
    sqc = req;

    // Asynchronous Event Request
    debugprint_command("ADMIN   | Asynchronous Event Request");

    createResponse();
  }
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
