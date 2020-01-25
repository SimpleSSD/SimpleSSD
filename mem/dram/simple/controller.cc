// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/dram/simple/controller.hh"

#include "mem/dram/simple/simple.hh"

namespace SimpleSSD::Memory::DRAM::Simple {

Controller::Controller(ObjectData &o, SimpleDRAM *p, Timing *t)
    : Object(o), parent(p), requestDepth(0), commandDepth(0) {
  // Get configs
  auto ctrl = object.config->getDRAMController();

  // This DRAM model does not have separate request queue
  maxRequestDepth = ctrl->readQueueSize + ctrl->writeQueueSize;
  maxCommandDepth = maxRequestDepth;

  // Create Rank
  auto dram = object.config->getDRAM();

  ranks.reserve(dram->rank);

  for (uint8_t i = 0; i < dram->rank; i++) {
    ranks.emplace_back(Rank(object, this, t));
  }

  // Create event
  eventWork = createEvent([this](uint64_t t, uint64_t) { work(t); },
                          "Memory::DRAM::Simple::Controller::eventWork");
}

Controller::~Controller() {}

void Controller::work(uint64_t now) {}

bool Controller::submit(Packet *pkt) noexcept {
  panic_if(pkt->opcode != Command::Read && pkt->opcode != Command::Write,
           "Invalid opcode.");

  if (requestDepth < maxRequestDepth) {
    requestDepth++;
    requestQueue.emplace_back(pkt);

    if (!isScheduled(eventWork)) {
      scheduleNow(eventWork);
    }

    return true;
  }

  return false;
}

void Controller::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Controller::getStatValues(std::vector<double> &) noexcept {}

void Controller::resetStatValues() noexcept {}

void Controller::createCheckpoint(std::ostream &) const noexcept {}

void Controller::restoreCheckpoint(std::istream &) noexcept {}

Packet *Controller::restorePacket(Packet *oldpkt) noexcept {
  // TODO:
}

}  // namespace SimpleSSD::Memory::DRAM::Simple
