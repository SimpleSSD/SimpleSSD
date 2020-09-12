// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 *         Junhyeok Jang <jhjang@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_BASE_ABSTRACT_FTL_HH__
#define __SIMPLESSD_FTL_BASE_ABSTRACT_FTL_HH__

#include "fil/fil.hh"
#include "ftl/def.hh"
#include "ftl/gc/hint.hh"
#include "ftl/object.hh"
#include "sim/object.hh"

namespace SimpleSSD::FTL {

class FTL;

class AbstractFTL : public Object {
 private:
  FTL *pFTL;

 protected:
  FTLObjectData &ftlobject;

  FIL::FIL *pFIL;

  void completeRequest(Request *);

 public:
  AbstractFTL(ObjectData &, FTLObjectData &, FTL *);
  virtual ~AbstractFTL();

  // TODO: ADD COMMENTS HERE!

  uint64_t generateFTLTag();
  Request *getRequest(uint64_t);

  void getGCHint(GC::HintContext &ctx) noexcept;

  virtual void initialize();

  virtual void read(Request *) = 0;
  virtual bool write(Request *) = 0;
  virtual void invalidate(Request *) = 0;

  virtual void restartStalledRequests() = 0;

  // In initialize phase of mapping, they may want to write spare area
  void writeSpare(PPN ppn, uint8_t *buffer, uint64_t size) {
    pFIL->writeSpare(ppn, buffer, size);
  }
};

}  // namespace SimpleSSD::FTL

#endif
