// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_JOB_MANAGER_HH__
#define __SIMPLESSD_FTL_JOB_MANAGER_HH__

#include "ftl/object.hh"
#include "sim/object.hh"

namespace SimpleSSD::FTL {

class AbstractJob : public Object {
 protected:
  FTLObjectData &ftlobject;

 public:
  AbstractJob(ObjectData &o, FTLObjectData &fo) : Object(o), ftlobject(fo) {}
  virtual ~AbstractJob() {}

  virtual void trigger_readMapping() {}
  virtual void trigger_readSubmit() {}
  virtual void trigger_readDone() {}
  virtual void trigger_writeMapping() {}
  virtual void trigger_writeSubmit() {}
  virtual void trigger_writeDone() {}
};

class JobManager : public Object {
 protected:
  std::vector<AbstractJob *> jobs;

 public:
  JobManager(ObjectData &);
  ~JobManager();

  /**
   * \brief Add FTL job to job manager
   *
   * This function must call in constructor of FTL.
   */
  void addJob(AbstractJob *);

  void trigger_readMapping();
  void trigger_readSubmit();
  void trigger_readDone();
  void trigger_writeMapping();
  void trigger_writeSubmit();
  void trigger_writeDone();

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL

#endif
