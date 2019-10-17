// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "fil/fil.hh"

#include "fil/pal/pal_wrapper.hh"

namespace SimpleSSD::FIL {

Request::Request()
    : id(0),
      eid(InvalidEventID),
      data(0),
      opcode(Operation::Read),
      address(0),
      buffer(nullptr) {}

Request::Request(uint64_t i, Event e, uint64_t d, Operation o, uint64_t a,
                 uint8_t *b)
    : id(i), eid(e), data(d), opcode(o), address(a), buffer(b) {}

Request::Request(uint64_t i, Event e, uint64_t d, Operation o, uint64_t a,
                 uint8_t *b, std::vector<uint8_t> &s)
    : id(i),
      eid(e),
      data(d),
      opcode(o),
      address(a),
      buffer(b),
      spare(std::move(s)) {}

FIL::FIL(ObjectData &o) : Object(o) {
  auto channel = readConfigUint(Section::FlashInterface, Config::Key::Channel);
  auto way = readConfigUint(Section::FlashInterface, Config::Key::Way);
  auto param = object.config->getNANDStructure();

  debugprint(Log::DebugID::FIL,
             "Channel |   Way   |   Die   |  Plane  |  Block  |   Page  ");
  debugprint(Log::DebugID::FIL, "%7u | %7u | %7u | %7u | %7u | %7u", channel,
             way, param->die, param->plane, param->block, param->page);
  debugprint(Log::DebugID::FIL, "Page size: %u + %u", param->pageSize,
             param->spareSize);

  switch ((Config::NVMType)readConfigUint(Section::FlashInterface,
                                          Config::Key::Model)) {
    case Config::NVMType::PAL:
      pFIL = new PALOLD(object, this);

      break;
    // case Config::NVMType::GenericNAND:
    default:
      panic("Unexpected FIL model.");

      break;
  }
}

FIL::~FIL() {
  delete pFIL;
}

void FIL::submit(Request &&req) {
  pFIL->enqueue(std::move(req));
}

void FIL::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  pFIL->getStatList(list, prefix + "fil.");
}

void FIL::getStatValues(std::vector<double> &values) noexcept {
  pFIL->getStatValues(values);
}

void FIL::resetStatValues() noexcept {
  pFIL->resetStatValues();
}

void FIL::createCheckpoint(std::ostream &out) const noexcept {
  pFIL->createCheckpoint(out);
}
void FIL::restoreCheckpoint(std::istream &in) noexcept {
  pFIL->restoreCheckpoint(in);
}

}  // namespace SimpleSSD::FIL
