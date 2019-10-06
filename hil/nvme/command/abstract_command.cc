// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/command/abstract_command.hh"

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

CommandData::CommandData(Subsystem *s, ControllerData *c)
    : subsystem(s),
      controller(c->controller),
      interface(c->interface),
      dma(c->dma),
      arbitrator(c->arbitrator),
      interrupt(c->interruptManager),
      memoryPageSize(c->memoryPageSize) {}

Command::Command(ObjectData &o, Subsystem *s, ControllerData *c)
    : Object(o), data(s, c), dmaEngine(nullptr), sqc(nullptr), cqc(nullptr) {}

Command::~Command() {}

void Command::createResponse() {
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
void Command::createDMAEngine(uint64_t size, Event eid) {
  panic_if(!sqc, "Request not submitted.");

  auto entry = sqc->getData();

  if (sqc->isSGL()) {
    dmaEngine = new SGLEngine(object, data.dma);
  }
  else {
    dmaEngine = new PRPEngine(object, data.dma, data.memoryPageSize);
  }

  dmaEngine->init(entry->dptr1, entry->dptr2, size, eid);
}

CQContext *Command::getResult() {
  panic_if(!cqc, "Response not created.");

  return cqc;
}

uint64_t Command::getUniqueID() {
  panic_if(!sqc, "Request not submitted.");

  // This ID is unique in subsystem
  return ((uint64_t)data.controller->getControllerID() << 32) | sqc->getID();
}

CommandData &Command::getCommandData() {
  return data;
}

void Command::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Command::getStatValues(std::vector<double> &) noexcept {}

void Command::resetStatValues() noexcept {}

void Command::createCheckpoint(std::ostream &out) noexcept {
  bool exist;

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
    uint16_t cqid = cqc->getCQID();

    BACKUP_BLOB(out, cqc->getData(), 16);
    // If we have cqc, we always have sqc. Just store data only.
  }
}

void Command::restoreCheckpoint(std::istream &in) noexcept {
  bool exist;

  RESTORE_SCALAR(in, exist);

  if (exist) {
    uint32_t id;

    RESTORE_SCALAR(in, id);

    sqc = data.arbitrator->getRecoveredRequest(id);

    panic_if(!sqc, "Invalid SQContext found while recover command status.");
  }

  RESTORE_SCALAR(in, exist);

  if (exist) {
    cqc = new CQContext();

    cqc->update(sqc);
    RESTORE_BLOB(in, cqc->getData(), 16);
  }
}

}  // namespace SimpleSSD::HIL::NVMe
