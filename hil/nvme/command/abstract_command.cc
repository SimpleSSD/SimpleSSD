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

DMACommandData *Command::createDMATag(ControllerData *cdata, SQContext *sqc) {
  auto ret = new DMACommandData(object, this, cdata);

  ret->sqc = sqc;

  addTagToList(ret);

  return ret;
}

BufferCommandData *Command::createBufferTag(ControllerData *cdata,
                                            SQContext *sqc) {
  auto ret = new BufferCommandData(object, this, cdata);

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

DMACommandData *Command::findDMATag(uint64_t gcid) {
  return (DMACommandData *)findTag(gcid);
}

BufferCommandData *Command::findBufferTag(uint64_t gcid) {
  return (BufferCommandData *)findTag(gcid);
}

void Command::destroyTag(CommandTag tag) {
  auto key = tag->getGCID();
  auto iter = tagList.find(key);

  panic_if(iter == tagList.end(),
           "No such command is passed to this command handler.");

  tagList.erase(iter);

  delete tag;
}

void Command::addTagToList(CommandTag tag) {
  auto key = tag->getGCID();

  // 64bit command unique ID is unique across the SSD
  tagList.emplace(std::make_pair(key, tag));
}

void Command::completeRequest(CommandTag tag) {
  if (auto iotag = dynamic_cast<DMACommandData *>(tag)) {
    if (iotag->dmaTag != InvalidDMATag) {
      iotag->dmaEngine->deinit(iotag->dmaTag);
    }
  }

  destroyTag(tag);
}

void Command::createCheckpoint(std::ostream &out) const noexcept {
  uint64_t size = tagList.size();
  uint8_t type;

  BACKUP_SCALAR(out, size);

  for (auto &iter : tagList) {
    BACKUP_SCALAR(out, iter.first);

    // Store type of CommandData
    if (dynamic_cast<BufferCommandData *>(iter.second)) {
      type = 3;
    }
    else if (dynamic_cast<DMACommandData *>(iter.second)) {
      type = 2;
    }
    else if (dynamic_cast<CommandData *>(iter.second)) {
      type = 1;
    }
    else {
      // Actually, we cannot be here!
      abort();
    }

    BACKUP_SCALAR(out, type);

    iter.second->createCheckpoint(out);
  }
}

void Command::restoreCheckpoint(std::istream &in) noexcept {
  uint64_t size;
  uint8_t type;

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
    RESTORE_SCALAR(in, type);

    CommandTag newTag = InvalidCommandTag;

    switch (type) {
      case 1:
        newTag = new CommandData(object, this, cdata);
        break;
      case 2:
        newTag = new DMACommandData(object, this, cdata);
        break;
      case 3:
        newTag = new BufferCommandData(object, this, cdata);
        break;
      default:
        panic("Unexpected CommandTag type.");
    }

    // Restore CommandTag (SQContext recovered here!)
    newTag->restoreCheckpoint(in);

    // Insert to tagList
    tagList.emplace(std::make_pair(uid, newTag));
  }
}

}  // namespace SimpleSSD::HIL::NVMe
