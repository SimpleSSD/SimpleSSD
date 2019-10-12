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

class CommandData : public Object {
 private:
  friend Command;

  Controller *controller;
  Interface *interface;
  Arbitrator *arbitrator;
  InterruptManager *interrupt;
  DMAEngine *dmaEngine;

  SQContext *sqc;
  CQContext *cqc;
  DMATag dmaTag;

  CommandData(ObjectData &o, ControllerData *);

  void createResponse();
  void createDMAEngine(uint32_t, Event);

 public:
  uint64_t getUniqueID();
  CQContext *getResponse();
};

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

CQContext *CommandData::getResponse() {
  panic_if(!cqc, "Response not created.");

  return cqc;
}

uint64_t CommandData::getUniqueID() {
  panic_if(!sqc, "Request not submitted.");

  // This ID is unique in subsystem
  return ((uint64_t)controller->getControllerID() << 32) | sqc->getID();
}

CommandData::CommandData(ObjectData &o, ControllerData *c)
    : Object(o),
      controller(c->controller),
      interface(c->interface),
      arbitrator(c->arbitrator),
      interrupt(c->interruptManager),
      dmaEngine(c->dmaEngine) {}

Command::Command(ObjectData &o, Subsystem *s) : Object(o), subsystem(s) {}

Command::~Command() {}

void Command::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Command::getStatValues(std::vector<double> &) noexcept {}

void Command::resetStatValues() noexcept {}

}  // namespace SimpleSSD::HIL::NVMe
