// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "fil/fil.hh"

#include "fil/nvm/pal/pal_wrapper.hh"
#include "fil/scheduler/noop.hh"

namespace SimpleSSD::FIL {

FIL::FIL(ObjectData &o) : Object(o), requestCounter(0) {
  auto channel = readConfigUint(Section::FlashInterface, Config::Key::Channel);
  auto way = readConfigUint(Section::FlashInterface, Config::Key::Way);
  auto param = object.config->getNANDStructure();

  debugprint(Log::DebugID::FIL,
             "Channel |   Way   |   Die   |  Plane  |  Block  |   Page  ");
  debugprint(Log::DebugID::FIL, "%7u | %7u | %7u | %7u | %7u | %7u", channel,
             way, param->die, param->plane, param->block, param->page);
  debugprint(Log::DebugID::FIL, "Page size: %u + %u", param->pageSize,
             param->spareSize);

  // Create event first
  eventCompletion = createEvent([this](uint64_t t, uint64_t d) {
    completion(t, d);
  }, "FIL::FIL::eventCompletion");

  switch ((Config::NVMType)readConfigUint(Section::FlashInterface,
                                          Config::Key::Model)) {
    case Config::NVMType::PAL:
      pNVM = new NVM::PALOLD(object, eventCompletion);

      break;
    // case Config::NVMType::GenericNAND:
    default:
      panic("Unexpected NVM model.");

      break;
  }

  switch ((Config::SchedulerType)readConfigUint(Section::FlashInterface,
                                                Config::Key::Scheduler)) {
    case Config::SchedulerType::Noop:
      pScheduler = new Scheduler::Noop(object, pNVM);

      break;
    default:
      panic("Unexpected Scheduler.");

      break;
  }
}

FIL::~FIL() {
  delete pScheduler;
  delete pNVM;
}

void FIL::submit(Operation opcode, Request &&_req) {
  // Insert request into queue
  uint64_t tag = ++requestCounter;

  _req.opcode = opcode;
  _req.tag = tag;

  auto ret = requestQueue.emplace(tag, std::move(_req));

  panic_if(!ret.second, "Request ID conflict.");

  // Submit
  auto &req = ret.first->second;

  pScheduler->submit(&req);
}

void FIL::completion(uint64_t, uint64_t tag) {
  auto iter = requestQueue.find(tag);

  panic_if(iter == requestQueue.end(), "Unexpected request %" PRIx64 "h.", tag);

  // Complete
  auto &req = iter->second;

  scheduleNow(req.eid, req.data);

  // Remove request
  requestQueue.erase(iter);
}

void FIL::read(Request &&req) {
  submit(Operation::Read, std::move(req));
}

void FIL::program(Request &&req) {
  submit(Operation::Program, std::move(req));
}

void FIL::erase(Request &&req) {
  submit(Operation::Erase, std::move(req));
}

void FIL::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  pNVM->getStatList(list, prefix + "fil.nvm.");
  pScheduler->getStatList(list, prefix + "fil.scheduler.");
}

void FIL::getStatValues(std::vector<double> &values) noexcept {
  pNVM->getStatValues(values);
  pScheduler->getStatValues(values);
}

void FIL::resetStatValues() noexcept {
  pNVM->resetStatValues();
  pScheduler->resetStatValues();
}

void FIL::createCheckpoint(std::ostream &out) const noexcept {
  pNVM->createCheckpoint(out);
  pScheduler->createCheckpoint(out);
}
void FIL::restoreCheckpoint(std::istream &in) noexcept {
  pNVM->restoreCheckpoint(in);
  pScheduler->restoreCheckpoint(in);
}

}  // namespace SimpleSSD::FIL
