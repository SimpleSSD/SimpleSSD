// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/command.hh"

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

CommandData::CommandData(ObjectData &o, Command *p, ControllerData *c)
    : Object(o),
      parent(p),
      controller(c->controller),
      interface(c->interface),
      arbitrator(c->arbitrator),
      interrupt(c->interruptManager),
      dmaEngine(c->dmaEngine),
      sqc(nullptr),
      cqc(nullptr) {}

void CommandData::createResponse() {
  panic_if(!sqc, "Request not submitted.");

  cqc = new CQContext();

  panic_if(!cqc, "Out of memory.");

  cqc->update(sqc);
}

CQContext *CommandData::getResponse() {
  panic_if(!cqc, "Response not created.");

  return cqc;
}

uint64_t CommandData::getGCID() {
  panic_if(!sqc, "Request not submitted.");

  // This ID is unique in subsystem
  return MAKE_GCID(controller->getControllerID(), sqc->getCCID());
}

Command *CommandData::getParent() {
  return parent;
}

Arbitrator *CommandData::getArbitrator() {
  return arbitrator;
}

void CommandData::createCheckpoint(std::ostream &out) const noexcept {
  bool exist;

  // Backup only Command Unique ID
  // All SQContext are allocated only once (not copied) and it wiil be backup in
  // SimpleSSD::HIL::NVMe::Arbitrator.
  exist = sqc != nullptr;
  BACKUP_SCALAR(out, exist);

  if (exist) {
    uint32_t id = sqc->getCCID();

    BACKUP_SCALAR(out, id);
  }

  // Backup all cqc because we create it.
  exist = cqc != nullptr;
  BACKUP_SCALAR(out, exist);

  if (exist) {
    // If we have cqc, we always have sqc. Just store data only.
    BACKUP_BLOB(out, cqc->getData(), 16);
  }
}

void CommandData::restoreCheckpoint(std::istream &in) noexcept {
  bool exist;

  RESTORE_SCALAR(in, exist);

  if (exist) {
    uint32_t id;

    RESTORE_SCALAR(in, id);

    sqc = arbitrator->getRecoveredRequest(id);

    panic_if(!sqc, "Invalid SQContext found while recover command status.");
  }

  RESTORE_SCALAR(in, exist);

  if (exist) {
    cqc = new CQContext();

    cqc->update(sqc);
    RESTORE_BLOB(in, cqc->getData(), 16);
  }
}

DMACommandData::DMACommandData(ObjectData &o, Command *p, ControllerData *c)
    : CommandData(o, p, c),
      dmaTag(InvalidDMATag),
      _slba(0),
      _nlb(0),
      beginAt(0) {}

/**
 * Create DMAEngine for command handling
 *
 * \param[in] size  Expected data size (for PRPEngine)
 * \param[in] eid   DMA init callback
 */
void DMACommandData::createDMAEngine(uint32_t size, Event eid) {
  auto entry = sqc->getData();

  if (sqc->isSGL()) {
    dmaTag = dmaEngine->initFromSGL(entry->dptr1, entry->dptr2, size, eid,
                                    getGCID());
  }
  else {
    dmaTag = dmaEngine->initFromPRP(entry->dptr1, entry->dptr2, size, eid,
                                    getGCID());
  }
}

void DMACommandData::destroyDMAEngine() {
  dmaEngine->deinit(dmaTag);
  dmaTag = InvalidDMATag;
}

void DMACommandData::createCheckpoint(std::ostream &out) const noexcept {
  bool exist;

  CommandData::createCheckpoint(out);

  BACKUP_DMATAG(out, dmaTag);
  BACKUP_SCALAR(out, _slba);
  BACKUP_SCALAR(out, _nlb);
  BACKUP_SCALAR(out, beginAt);
}

void DMACommandData::restoreCheckpoint(std::istream &in) noexcept {
  bool exist;

  CommandData::restoreCheckpoint(in);

  RESTORE_DMATAG(dmaEngine, in, dmaTag);
  RESTORE_SCALAR(in, _slba);
  RESTORE_SCALAR(in, _nlb);
  RESTORE_SCALAR(in, beginAt);

  RESTORE_SCALAR(in, exist);
}

BufferCommandData::BufferCommandData(ObjectData &o, Command *p,
                                     ControllerData *c)
    : DMACommandData(o, p, c), complete(0) {}

void BufferCommandData::createCheckpoint(std::ostream &out) const noexcept {
  DMACommandData::createCheckpoint(out);

  BACKUP_SCALAR(out, complete);

  uint64_t size = buffer.size();
  BACKUP_SCALAR(out, size);

  if (size > 0) {
    BACKUP_BLOB(out, buffer.data(), size);
  }
}

void BufferCommandData::restoreCheckpoint(std::istream &in) noexcept {
  uint64_t size;

  DMACommandData::restoreCheckpoint(in);

  RESTORE_SCALAR(in, complete);
  RESTORE_SCALAR(in, size);

  if (size > 0) {
    buffer.resize(size);

    RESTORE_BLOB(in, buffer.data(), size);
  }
}

}  // namespace SimpleSSD::HIL::NVMe
