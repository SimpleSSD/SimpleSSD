// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_BACKGROUND_MANAGER_ABSTRACT_JOB_MANAGER_HH__
#define __SIMPLESSD_FTL_BACKGROUND_MANAGER_ABSTRACT_JOB_MANAGER_HH__

#include "ftl/background_manager/abstract_background_job.hh"

namespace SimpleSSD::FTL {

class AbstractJobManager : public Object {
 public:
  AbstractJobManager(ObjectData &o) : Object(o) {}
  virtual ~AbstractJobManager() {}

  /**
   * \brief Register background job to job manager
   *
   * \param pjob Pointer to job object
   */
  virtual void addBackgroundJob(AbstractJob *pjob) = 0;

  /**
   * \brief Initialize registered background job
   *
   * \param[in] restore True if restore state from checkpoint
   */
  virtual void initialize(bool restore) = 0;

  /**
   * \brief Check any of background job is running
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
  virtual void triggerByUser(TriggerType when, Request *req) = 0;
};

}  // namespace SimpleSSD::FTL

#endif
