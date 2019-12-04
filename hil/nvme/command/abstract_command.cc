// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/abstract_command.hh"

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

Command::Command(ObjectData &o, Subsystem *s) : Object(o), subsystem(s) {}

CommandTag Command::createTag(ControllerData *cdata, SQContext *sqc) {
  auto ret = new CommandData(object, this, cdata);

  ret->sqc = sqc;

  addTagToList(ret);

  return ret;
}

CommandTag Command::findTag(uint64_t gcid) {
  auto iter = tagList.find(gcid);

  panic_if(iter == tagList.end(),
           "No such command is passed to this command handler.");

  return iter->second;
}

void Command::destroyTag(CommandTag tag) {
  auto key = MAKE64(tag->controller->getControllerID(), tag->cqc->getCCID());
  auto iter = tagList.find(key);

  panic_if(iter == tagList.end(),
           "No such command is passed to this command handler.");

  tagList.erase(iter);

  delete tag;
}

void Command::addTagToList(CommandTag tag) {
  auto key = tag->getGCID();

  // 64bit command unique ID is unique across the SSD
  tagList.emplace(key, tag);
}

void Command::completeRequest(CommandTag tag) {
  destroyTag(tag);
}

void Command::createCheckpoint(std::ostream &out) const noexcept {
  uint64_t size = tagList.size();

  BACKUP_SCALAR(out, size);

  for (auto &iter : tagList) {
    BACKUP_SCALAR(out, iter.first);

    iter.second->createCheckpoint(out);
  }
}

void Command::restoreCheckpoint(std::istream &in) noexcept {
  uint64_t size;

  RESTORE_SCALAR(in, size);

  tagList.reserve(size);

  for (uint64_t i = 0; i < size; i++) {
    uint64_t uid;

    RESTORE_SCALAR(in, uid);

    // Find controller and request controller data
    ControllerID ctrlid = (ControllerID)(uid >> 32);

    auto ctrl = (NVMe::Controller *)subsystem->getController(ctrlid);
    auto cdata = ctrl->getControllerData();

    // Regenerate CommandTag
    CommandTag newTag = new CommandData(object, this, cdata);

    // Restore CommandTag (SQContext recovered here!)
    newTag->restoreCheckpoint(in);

    // Insert to tagList
    tagList.emplace(uid, newTag);
  }
}

}  // namespace SimpleSSD::HIL::NVMe
