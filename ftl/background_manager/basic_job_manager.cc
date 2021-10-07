// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/background_manager/basic_job_manager.hh"

namespace SimpleSSD::FTL {

BasicJobManager::BasicJobManager(ObjectData &o)
    : AbstractJobManager(o),
      threshold(readConfigUint(Section::FlashTranslation,
                               Config::Key::IdleTimeThreshold)),
      lastScheduledAt(0) {
  eventIdletime =
      createEvent([this](uint64_t t, uint64_t) { idletimeEvent(t); },
                  "FTL::BasicJobManager::eventIdletime");

  rescheduleIdletimeDetection(0);

  resetStatValues();
}

BasicJobManager::~BasicJobManager() {
  for (const auto &iter : jobs) {
    delete iter;
  }
}

void BasicJobManager::idletimeEvent(uint64_t now) {
  stat.count++;

  for (auto &iter : jobs) {
    // Threshold based idletime detection has no deadline
    iter->triggerByIdle(now, std::numeric_limits<uint64_t>::max());
  }
}

void BasicJobManager::addBackgroundJob(AbstractJob *pjob) {
  jobs.emplace_back(pjob);
}

void BasicJobManager::initialize() {
  for (auto &iter : jobs) {
    iter->initialize();
  }
}

bool BasicJobManager::isRunning() {
  for (auto &iter : jobs) {
    if (iter->isRunning()) {
      return true;
    }
  }

  return false;
}

void BasicJobManager::triggerByUser(TriggerType when, Request *req) {
  uint64_t now = getTick();

  for (auto &iter : jobs) {
    iter->triggerByUser(when, req);

    if (iter->isRunning()) {
      break;
    }
  }

  switch (when) {
    case TriggerType::ReadMapping:
    case TriggerType::WriteMapping:
      markUserMapping(now);

      // We got request -- reset idletime timer
      descheduleIdletimeDetection();

      break;
    case TriggerType::ReadComplete:
    case TriggerType::WriteComplete:
      markUserComplete(now);

      // Current request has been completed, restart timer
      rescheduleIdletimeDetection(now);

      break;
    default:
      break;
  }
}

void BasicJobManager::getStatList(std::vector<Stat> &list,
                                  std::string prefix) noexcept {
  list.emplace_back(prefix + "count", "Assigned FTL background job count");
  list.emplace_back(
      prefix + "idletime.count",
      "Total number of background job invocation by idletime detection");
  list.emplace_back(prefix + "idletime.usable", "Total usable idletime in ms.");
  list.emplace_back(prefix + "idletime.wasted", "Total wasted idletime in ms.");

  for (auto &iter : jobs) {
    iter->getStatList(list, prefix);
  }
}

void BasicJobManager::getStatValues(std::vector<double> &values) noexcept {
  values.emplace_back((double)jobs.size());
  values.emplace_back((double)stat.count);
  values.emplace_back((double)stat.usable / 1000000000.);
  values.emplace_back((double)stat.wasted / 1000000000.);

  for (auto &iter : jobs) {
    iter->getStatValues(values);
  }
}

void BasicJobManager::resetStatValues() noexcept {
  stat.count = 0;
  stat.usable = 0;
  stat.wasted = 0;

  for (auto &iter : jobs) {
    iter->resetStatValues();
  }
}

void BasicJobManager::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_STL(out, jobs, iter, { iter->createCheckpoint(out); });

  BACKUP_SCALAR(out, lastScheduledAt);
  BACKUP_SCALAR(out, lastCompleteAt);
  BACKUP_SCALAR(out, stat);

  BACKUP_EVENT(out, eventIdletime);
}

void BasicJobManager::restoreCheckpoint(std::istream &in) noexcept {
  uint64_t size;

  RESTORE_SCALAR(in, size);

  panic_if(
      size != jobs.size(),
      "Unexpected number of background jobs while restore from checkpoint.");

  for (auto &iter : jobs) {
    iter->restoreCheckpoint(in);
  }

  RESTORE_SCALAR(in, lastScheduledAt);
  RESTORE_SCALAR(in, lastCompleteAt);
  RESTORE_SCALAR(in, stat);

  RESTORE_EVENT(in, eventIdletime);
}

}  // namespace SimpleSSD::FTL
