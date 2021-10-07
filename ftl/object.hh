// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_OBJECT_HH__
#define __SIMPLESSD_FTL_OBJECT_HH__

namespace SimpleSSD::FTL {

class AbstractFTL;
class AbstractJobManager;

namespace Mapping {

class AbstractMapping;

}

namespace BlockAllocator {

class AbstractAllocator;

}

//! Encapsulates all FTL models
struct FTLObjectData {
  AbstractFTL *pFTL;
  Mapping::AbstractMapping *pMapping;
  BlockAllocator::AbstractAllocator *pAllocator;
  AbstractJobManager *pJobManager;

  FTLObjectData()
      : pFTL(nullptr),
        pMapping(nullptr),
        pAllocator(nullptr),
        pJobManager(nullptr) {}
};

}  // namespace SimpleSSD::FTL

#endif
