// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_BACKGROUND_MANAGER_ABSTRACT_BACKGROUND_JOB_HH__
#define __SIMPLESSD_FTL_BACKGROUND_MANAGER_ABSTRACT_BACKGROUND_JOB_HH__

#include "ftl/def.hh"
#include "ftl/object.hh"
#include "sim/object.hh"

namespace SimpleSSD::FTL {

enum class TriggerType : uint32_t {
  ReadMapping,    // Before accessing mapping table
  ReadSubmit,     // After accessing mapping table, before FIL read submission
  ReadComplete,   // After FIL completion
  WriteMapping,   // Before updating mapping table
  WriteSubmit,    // After updating mapping table, before FIL write submission
  WriteComplete,  // After FIL completion

  // TODO: TRIM/Format?
};

class AbstractJob : public Object {
 protected:
  FTLObjectData &ftlobject;

 public:
  AbstractJob(ObjectData &, FTLObjectData &);
  virtual ~AbstractJob();

  /**
   * \brief Initialize abstract job
   *
   * This function will be called after all objects in FTLObjectData have been
   * initialized.
   */
  virtual void initialize() = 0;

  /**
   * \brief Query job is running
   *
   * \return true if job is running
   */
  virtual bool isRunning() = 0;

  /**
   * \brief Triggered by user I/O
   *
   * \param when Trigger type.
   * \param req  Request that triggered this event.
   */
  virtual void triggerByUser(TriggerType when, Request *req);

  /**
   * \brief Triggered by SSD idleness
   *
   * \param now      Current simulation tick
   * \param deadline Expected timestamp of next user I/O
   */
  virtual void triggerByIdle(uint64_t now, uint64_t deadline);
};

}  // namespace SimpleSSD::FTL

#endif
