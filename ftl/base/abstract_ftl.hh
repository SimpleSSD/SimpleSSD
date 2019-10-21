// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_BASE_ABSTRACT_FTL_HH__
#define __SIMPLESSD_FTL_BASE_ABSTRACT_FTL_HH__

#include "fil/fil.hh"
#include "ftl/allocator/abstract_allocator.hh"
#include "ftl/def.hh"
#include "ftl/mapping/abstract_mapping.hh"
#include "hil/command_manager.hh"
#include "sim/object.hh"

namespace SimpleSSD::FTL {

class AbstractFTL : public Object {
 protected:
  CommandManager *commandManager;
  FIL::FIL *pFIL;

  Mapping::AbstractMapping *pMapper;
  BlockAllocator::AbstractAllocator *pAllocator;

  uint64_t ftlCommandTag;

  inline uint64_t makeFTLCommandTag() {
    return ftlCommandTag++ | FTL_TAG_PREFIX;
  }

 public:
  AbstractFTL(ObjectData &o, CommandManager *c, FIL::FIL *f,
              Mapping::AbstractMapping *m, BlockAllocator::AbstractAllocator *a)
      : Object(o), commandManager(c), pFIL(f), pMapper(m), pAllocator(a) {}
  AbstractFTL(const AbstractFTL &) = delete;
  AbstractFTL(AbstractFTL &&) noexcept = default;
  ~AbstractFTL() {}

  AbstractFTL &operator=(const AbstractFTL &) = delete;
  AbstractFTL &operator=(AbstractFTL &&) = default;

  virtual void initialize() {}

  virtual void submit(uint64_t) = 0;
  virtual bool isGC() = 0;
  virtual uint8_t isFormat() = 0;

  // In initialize phase of mapping, they may want to write spare area
  void writeSpare(PPN ppn, std::vector<uint8_t> &spare) {
    pFIL->writeSpare(ppn, spare);
  }
};

}  // namespace SimpleSSD::FTL

#endif
