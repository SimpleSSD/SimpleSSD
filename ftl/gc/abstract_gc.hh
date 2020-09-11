// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_GC_ABSTRACT_GC_HH__
#define __SIMPLESSD_FTL_GC_ABSTRACT_GC_HH__

#include "sim/object.hh"
#include "ftl/def.hh"

namespace SimpleSSD::FTL {

class AbstractFTL;

namespace GC {

class AbstractGC : public Object {
 protected:
  AbstractFTL *pFTL;

  const Parameter *param;

 public:
  AbstractGC(ObjectData &o);
  virtual ~AbstractGC();

  virtual void initialize(AbstractFTL *f, const Parameter *p) {
    pFTL = f;
    param = p;
  }

  /**
   * \brief Trigger foreground GC if condition met
   */
  virtual void triggerForeground() = 0;

  /**
   * \brief Notify request arrived (background GC)
   */
  virtual void requestArrived() = 0;

  /**
   * \brief Block serving write request
   */
  virtual bool stopWrite() = 0;
};

}  // namespace GC

}  // namespace SimpleSSD::FTL

#endif
