// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_GC_ABSTRACT_GC_HH__
#define __SIMPLESSD_FTL_GC_ABSTRACT_GC_HH__

#include "ftl/def.hh"
#include "ftl/gc/hint.hh"
#include "ftl/object.hh"
#include "sim/object.hh"

namespace SimpleSSD::FTL::GC {

class AbstractGC : public Object {
 protected:
  FTLObjectData &ftlobject;

  const Parameter *param;

 public:
  AbstractGC(ObjectData &, FTLObjectData &);
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
};

}  // namespace SimpleSSD::FTL::GC

#endif
