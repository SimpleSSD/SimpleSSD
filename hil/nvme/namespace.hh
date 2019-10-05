// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __HIL_NVME_NAMESPACE_HH__
#define __HIL_NVME_NAMESPACE_HH__

#include "sim/object.hh"

namespace SimpleSSD::HIL::NVMe {

class NamespaceInformation {
 public:
  uint64_t size;                         //!< NSZE
  uint64_t capacity;                     //!< NCAP
  uint64_t utilization;                  //!< NUSE
  uint64_t sizeInByteL;                  //<! NVMCAPL
  uint64_t sizeInByteH;                  //<! NVMCAPH
  uint8_t lbaFormatIndex;                //!< FLBAS
  uint8_t dataProtectionSettings;        //!< DPS
  uint8_t namespaceSharingCapabilities;  //!< NMIC
  uint8_t nvmSetIdentifier;              //!< NVMSETID
  uint32_t anaGroupIdentifier;           //!< ANAGRPID

  uint32_t lbaSize;
  uint64_t lpnBegin;
  uint64_t lpnEnd;
} Information;

class Namespace : public Object {};

}  // namespace SimpleSSD::HIL::NVMe

#endif
