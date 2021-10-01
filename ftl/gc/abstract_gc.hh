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
#include "ftl/job_manager.hh"

namespace SimpleSSD::FTL::GC {

class AbstractGC : public AbstractJob {
 protected:
  FIL::FIL *pFIL;

  const Parameter *param;

 public:
  AbstractGC(ObjectData &, FTLObjectData &, FIL::FIL *);
  virtual ~AbstractGC();

  void trigger_readMapping(Request *req) final { requestArrived(req); }
  void trigger_readSubmit(Request *) final {}
  void trigger_readDone(Request *) final {}
  void trigger_writeMapping(Request *req) final { requestArrived(req); }
  void trigger_writeSubmit(Request *) final {}
  void trigger_writeDone(Request *req) final { triggerForeground(); }

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
  virtual void requestArrived(Request *) = 0;
};

}  // namespace SimpleSSD::FTL::GC

#endif
