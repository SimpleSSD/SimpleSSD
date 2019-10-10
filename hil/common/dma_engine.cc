// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/common/dma_engine.hh"

namespace SimpleSSD::HIL {

DMAEngine::DMAEngine(ObjectData &o, DMAInterface *i)
    : Object(o), interface(i) {
  eventDMADone = createEvent([this](uint64_t) { dmaDone(); },
                           "HIL::DMAEngine::dmaHandler");
}

DMAEngine::~DMAEngine() {}

void DMAEngine::dmaDone() {
  auto tag = pendingTagList.front();

  tag->counter--;

  if (tag->counter == 0) {
    pendingTagList.pop_front();

    schedule(tag->eid);
  }
}

DMATag DMAEngine::createTag() {
  auto tag = new DMAData();

  tagList.emplace(tag);

  return tag;
}

void DMAEngine::destroyTag(DMATag tag) {
  auto iter = tagList.find(tag);

  if (iter != tagList.end()) {
    tagList.erase(iter);
  }

  delete tag;
}

void DMAEngine::getStatList(std::vector<Stat> &, std::string) noexcept {}

void DMAEngine::getStatValues(std::vector<double> &) noexcept {}

void DMAEngine::resetStatValues() noexcept {}

void DMAEngine::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_EVENT(out, eventDMADone);

  uint64_t size = tagList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : tagList) {
    BACKUP_DMATAG(out, iter);

    BACKUP_EVENT(out, iter->eid);
    BACKUP_SCALAR(out, iter->counter);

    uint64_t size = iter->prList.size();
    BACKUP_SCALAR(out, size);

    for (auto &pr : iter->prList) {
      BACKUP_SCALAR(out, pr.address);
      BACKUP_SCALAR(out, pr.size);
      BACKUP_SCALAR(out, pr.ignore);
    }
  }

  size = pendingTagList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : pendingTagList) {
    BACKUP_DMATAG(out, iter);
  }
}

void DMAEngine::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_EVENT(in, eventDMADone);

  uint64_t size;
  DMATag oldTag, newTag;

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    newTag = new DMAData();

    RESTORE_SCALAR(in, oldTag);

    RESTORE_EVENT(in, newTag->eid);
    RESTORE_SCALAR(in, newTag->counter);

    uint64_t list;
    RESTORE_SCALAR(in, list);

    for (uint64_t j = 0; j < list; j++) {
      PhysicalRegion pr;

      RESTORE_SCALAR(in, pr.address);
      RESTORE_SCALAR(in, pr.size);
      RESTORE_SCALAR(in, pr.ignore);

      newTag->prList.emplace_back(pr);
    }

    oldTagList.emplace(std::make_pair(oldTag, newTag));
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    RESTORE_SCALAR(in, oldTag);
    oldTag = restoreDMATag(oldTag);

    pendingTagList.push_back(oldTag);
  }
}

DMATag DMAEngine::restoreDMATag(DMATag oldTag) noexcept {
  auto iter = oldTagList.find(oldTag);

  if (iter == oldTagList.end()) {
    panic_log("Tag not found");
  }

  return iter->second;
}

void DMAEngine::clearOldDMATagList() noexcept {
  oldTagList.clear();
}

}  // namespace SimpleSSD::HIL
