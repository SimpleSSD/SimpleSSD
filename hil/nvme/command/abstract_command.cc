// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/abstract_command.hh"

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
      cqc(nullptr),
      dmaTag(InvalidDMATag) {}

CommandData::~CommandData() {
  // Must not delete sqc
  delete cqc;

  if (dmaTag != InvalidDMATag) {
    panic("DMA not uninitialized.");
  }
}

void CommandData::createResponse() {
  panic_if(!sqc, "Request not submitted.");

  cqc = new CQContext();

  panic_if(!cqc, "Out of memory.");

  cqc->update(sqc);
}

/**
 * Create DMAEngine for command handling
 *
 * \param[in] size  Expected data size (for PRPEngine)
 * \param[in] eid   DMA init callback
 */
void CommandData::createDMAEngine(uint32_t size, Event eid) {
  auto entry = sqc->getData();

  if (sqc->isSGL()) {
    dmaTag = dmaEngine->initFromSGL(entry->dptr1, entry->dptr2, size, eid);
  }
  else {
    dmaTag = dmaEngine->initFromPRP(entry->dptr1, entry->dptr2, size, eid);
  }
}

void CommandData::destroyDMAEngine() {
  dmaEngine->deinit(dmaTag);
  dmaTag = InvalidDMATag;
}

CQContext *CommandData::getResponse() {
  panic_if(!cqc, "Response not created.");

  return cqc;
}

uint64_t CommandData::getUniqueID() {
  panic_if(!sqc, "Request not submitted.");

  // This ID is unique in subsystem
  return ((uint64_t)controller->getControllerID() << 32) | sqc->getID();
}

Command *CommandData::getParent() {
  return parent;
}

Arbitrator *CommandData::getArbitrator() {
  return arbitrator;
}

void CommandData::createCheckpoint(std::ostream &out) const noexcept {
  bool exist;

  BACKUP_DMATAG(out, dmaTag);

  // Backup only Command Unique ID
  // All SQContext are allocated only once (not copied) and it wiil be backup in
  // SimpleSSD::HIL::NVMe::Arbitrator.
  exist = sqc != nullptr;
  BACKUP_SCALAR(out, exist);

  if (exist) {
    uint32_t id = sqc->getID();

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

  RESTORE_DMATAG(dmaEngine, in, dmaTag);

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

Command::Command(ObjectData &o, Subsystem *s) : Object(o), subsystem(s) {}

Command::~Command() {}

CommandTag Command::createTag(ControllerData *cdata) {
  return new CommandData(object, this, cdata);
}

}  // namespace SimpleSSD::HIL::NVMe
