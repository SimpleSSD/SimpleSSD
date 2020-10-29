// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NVME_NAMESPACE_HH__
#define __SIMPLESSD_HIL_NVME_NAMESPACE_HH__

#include "hil/nvme/abstract_namespace.hh"

namespace SimpleSSD ::HIL::NVMe {

class Namespace : public AbstractNamespace {
 public:
  Namespace(ObjectData &);
  ~Namespace();

  CommandSetIdentifier getCommandSetIdentifier() override {
    return CommandSetIdentifier::NVM;
  };

  bool validateCommand(ControllerID, SQContext *, CQContext *) override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
