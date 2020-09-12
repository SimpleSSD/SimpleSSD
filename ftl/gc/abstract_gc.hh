// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_GC_ABSTRACT_GC_HH__
#define __SIMPLESSD_FTL_GC_ABSTRACT_GC_HH__

#include "fil/fil.hh"
#include "ftl/def.hh"
#include "ftl/gc/hint.hh"
#include "ftl/object.hh"
#include "sim/object.hh"

namespace SimpleSSD::FTL::GC {

class AbstractGC : public Object {
 protected:
  FTLObjectData &ftlobject;
  FIL::FIL *pFIL;

  const Parameter *param;

  uint64_t requestCounter;
  std::unordered_map<uint64_t, CopyContext> ongoingCopy;

  inline uint64_t getGCTag() noexcept { return requestCounter++; }

  inline uint64_t startCopySession(CopyContext &&ctx) noexcept {
    auto ret = ongoingCopy.emplace(getGCTag(), std::move(ctx));

    panic_if(!ret.second, "Unexpected tag colision.");

    return ret.first->first;
  }

  inline CopyContext &findCopySession(uint64_t tag) noexcept {
    auto iter = ongoingCopy.find(tag);

    panic_if(iter == ongoingCopy.end(), "Unexpected copy tag.");

    return iter->second;
  }

  inline void closeCopySession(uint64_t tag) noexcept {
    auto iter = ongoingCopy.find(tag);

    panic_if(iter == ongoingCopy.end(), "Unexpected copy tag.");

    ongoingCopy.erase(iter);
  }

  inline uint64_t getSessionCount() noexcept { return ongoingCopy.size(); }

 public:
  AbstractGC(ObjectData &, FTLObjectData &, FIL::FIL *);
  virtual ~AbstractGC();

  /**
   * \brief GC initialization function
   *
   * Immediately call AbstractGC::initialize() when you override this function.
   */
  virtual void initialize();

  /**
   * \brief Trigger foreground GC if condition met
   */
  virtual void triggerForeground() = 0;

  /**
   * \brief Notify request arrived (background GC)
   */
  virtual void requestArrived(bool read, uint32_t nlp) = 0;

  /**
   * \brief Check write request should be stalled
   */
  virtual bool checkWriteStall() = 0;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL::GC

#endif
