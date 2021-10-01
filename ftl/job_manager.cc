// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/job_manager.hh"

namespace SimpleSSD::FTL {

JobManager::JobManager(ObjectData &o) : Object(o) {}

JobManager::~JobManager() {
  for (const auto &iter : jobs) {
    delete iter;
  }
}

void JobManager::addJob(AbstractJob *pjob) {
  jobs.emplace_back(pjob);
}

void JobManager::initialize() {
  for (auto &iter : jobs) {
    iter->initialize();
  }
}

void JobManager::trigger_readMapping(Request *req) {
  for (auto &iter : jobs) {
    iter->trigger_readMapping(req);
  }
}

void JobManager::trigger_readSubmit(Request *req) {
  for (auto &iter : jobs) {
    iter->trigger_readSubmit(req);
  }
}

void JobManager::trigger_readDone(Request *req) {
  for (auto &iter : jobs) {
    iter->trigger_readDone(req);
  }
}

void JobManager::trigger_writeMapping(Request *req) {
  for (auto &iter : jobs) {
    iter->trigger_writeMapping(req);
  }
}

void JobManager::trigger_writeSubmit(Request *req) {
  for (auto &iter : jobs) {
    iter->trigger_writeSubmit(req);
  }
}

void JobManager::trigger_writeDone(Request *req) {
  for (auto &iter : jobs) {
    iter->trigger_writeDone(req);
  }
}

void JobManager::getStatList(std::vector<Stat> &list,
                             std::string prefix) noexcept {
  list.emplace_back(prefix + "count", "Assigned FTL background job count");

  for (auto &iter : jobs) {
    iter->getStatList(list, prefix);
  }
}

void JobManager::getStatValues(std::vector<double> &values) noexcept {
  values.emplace_back((double)jobs.size());

  for (auto &iter : jobs) {
    iter->getStatValues(values);
  }
}

void JobManager::resetStatValues() noexcept {
  for (auto &iter : jobs) {
    iter->resetStatValues();
  }
}

void JobManager::createCheckpoint(std::ostream &out) const noexcept {
  for (auto &iter : jobs) {
    iter->createCheckpoint(out);
  }
}

void JobManager::restoreCheckpoint(std::istream &in) noexcept {
  for (auto &iter : jobs) {
    iter->restoreCheckpoint(in);
  }
}

}  // namespace SimpleSSD::FTL
