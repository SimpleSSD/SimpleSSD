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

void CommandData::initRequest(Event eid) {
  request = Request(eid, getGCID());
}

void CommandData::makeResponse() {
  panic_if(!cqc, "Response not created.");

  auto response = request.getResponse();

  switch (response) {
    case Response::Unwritten:
      break;
    case Response::OutOfRange:
      break;
    case Response::FormatInProgress:
      break;
    case Response::ReadECCFail:
      break;
    case Response::WriteFail:
      break;
    case Response::CompareFail:
      cqc->makeStatus(false, false, StatusType::MediaAndDataIntegrityErrors,
                      MediaAndDataIntegrityErrorCode::CompareFailure);

      break;
    default:
      // Do nothing
      break;
  }
}

void CommandData::createDMAEngine(uint32_t size, Event eid) {
  auto entry = sqc->getData();
  DMATag dmaTag;

  if (sqc->isSGL()) {
    dmaTag = dmaEngine->initFromSGL(entry->dptr1, entry->dptr2, size, eid,
                                    getGCID());
  }
  else {
    dmaTag = dmaEngine->initFromPRP(entry->dptr1, entry->dptr2, size, eid,
                                    getGCID());
  }

  request.setDMA(dmaEngine, dmaTag);
}

void CommandData::destroyDMAEngine() {
  auto tag = request.getDMA();

  if (tag != InvalidDMATag) {
    dmaEngine->deinit(request.getDMA());
  }
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

  request.createCheckpoint(out);

  // Backup DMATag
  DMATag tag = request.getDMA();

  BACKUP_DMATAG(out, tag);
  BACKUP_SCALAR(out, beginAt);

  uint64_t size = buffer.size();
  BACKUP_SCALAR(out, size);

  if (size > 0) {
    BACKUP_BLOB(out, buffer.data(), size);
  }
}

void CommandData::restoreCheckpoint(std::istream &in) noexcept {
  bool exist;

  RESTORE_SCALAR(in, exist);

  if (exist) {
    uint32_t id;

    RESTORE_SCALAR(in, id);

    sqc = arbitrator->restoreRequest(id);

    panic_if(!sqc, "Invalid SQContext found while recover command status.");
  }

  RESTORE_SCALAR(in, exist);

  if (exist) {
    cqc = new CQContext();

    cqc->update(sqc);
    RESTORE_BLOB(in, cqc->getData(), 16);
  }

  request.restoreCheckpoint(in, object);

  // Restore DMATag
  DMATag tag;

  RESTORE_DMATAG(dmaEngine, in, tag);
  RESTORE_SCALAR(in, beginAt);

  request.setDMA(dmaEngine, tag);

  uint64_t size;
  RESTORE_SCALAR(in, size);

  if (size > 0) {
    buffer.resize(size);
    RESTORE_BLOB64(in, buffer.data(), size);
  }
}

}  // namespace SimpleSSD::HIL::NVMe
