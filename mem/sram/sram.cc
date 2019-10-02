// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/sram/sram.hh"

namespace SimpleSSD::Memory::SRAM {

SRAM::SRAM(ObjectData &&o)
    : AbstractSRAM(std::move(o), "SimpleSSD::Memory::SRAM::SRAM") {
  // Convert cycle to ps
  pStructure->latency =
      (uint64_t)(pStructure->latency * 1000000000000.f /
                 readConfigUint(Section::CPU, CPU::Config::Clock));
}

SRAM::~SRAM() {}

uint64_t SRAM::preSubmit(Request *req) {
  // Calculate latency
  return DIVCEIL(req->length, pStructure->lineSize) * pStructure->latency;
}

void SRAM::postDone(Request *req) {
  // Call handler
  schedule(req->eid, getTick(), req->context);

  delete req;
}

uint64_t SRAM::preSubmitRead(void *data) {
  return preSubmit((Request *)data);
}

uint64_t SRAM::preSubmitWrite(void *data) {
  return preSubmit((Request *)data);
}

void SRAM::postReadDone(void *data) {
  postDone((Request *)data);
}

void SRAM::postWriteDone(void *data) {
  postDone((Request *)data);
}

void SRAM::read(uint64_t address, uint64_t length, Event eid, void *context) {
  auto req = new Request(address, length, eid, context);

  rangeCheck(address, length);

  // Enqueue request
  req->beginAt = getTick();

  readStat.count++;
  readStat.size += length;

  AbstractFIFO::read(req);
}

void SRAM::write(uint64_t address, uint64_t length, Event eid, void *context) {
  auto req = new Request(address, length, eid, context);

  rangeCheck(address, length);

  // Enqueue request
  req->beginAt = getTick();

  writeStat.count++;
  writeStat.size += length;

  AbstractFIFO::write(req);
}

}  // namespace SimpleSSD::Memory::SRAM
