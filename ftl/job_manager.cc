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

bool JobManager::trigger_readMapping(Request *req) {
  bool triggered = false;

  for (auto &iter : jobs) {
    triggered = iter->trigger_readMapping(req);

    if (triggered) {
      break;
    }
  }

  return triggered;
}

bool JobManager::trigger_readSubmit(Request *req) {
  bool triggered = false;

  for (auto &iter : jobs) {
    triggered = iter->trigger_readSubmit(req);

    if (triggered) {
      break;
    }
  }

  return triggered;
}

bool JobManager::trigger_readDone(Request *req) {
  bool triggered = false;

  for (auto &iter : jobs) {
    triggered = iter->trigger_readDone(req);

    if (triggered) {
      break;
    }
  }

  return triggered;
}

bool JobManager::trigger_writeMapping(Request *req) {
  bool triggered = false;

  for (auto &iter : jobs) {
    triggered = iter->trigger_writeMapping(req);

    if (triggered) {
      break;
    }
  }

  return triggered;
}

bool JobManager::trigger_writeSubmit(Request *req) {
  bool triggered = false;

  for (auto &iter : jobs) {
    triggered = iter->trigger_writeSubmit(req);

    if (triggered) {
      break;
    }
  }

  return triggered;
}

bool JobManager::trigger_writeDone(Request *req) {
  bool triggered = false;

  for (auto &iter : jobs) {
    triggered = iter->trigger_writeDone(req);

    if (triggered) {
      break;
    }
  }

  return triggered;
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
