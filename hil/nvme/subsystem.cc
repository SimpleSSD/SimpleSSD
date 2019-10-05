// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/subsystem.hh"

namespace SimpleSSD::HIL::NVMe {

const uint32_t nLBAFormat = 4;
const uint32_t lbaFormat[nLBAFormat] = {
    0x02090000,  // 512B + 0, Good performance
    0x020A0000,  // 1KB + 0, Good performance
    0x010B0000,  // 2KB + 0, Better performance
    0x000C0000,  // 4KB + 0, Best performance
};
const uint32_t lbaSize[nLBAFormat] = {
    512,   // 512B
    1024,  // 1KB
    2048,  // 2KB
    4096,  // 4KB
};

Subsystem::Subsystem(ObjectData &o)
    : AbstractSubsystem(o), allocatedLogicalPages(0) {
  // TODO: Get total physical page count from configuration
  // auto page = readConfigUint(Section::FTL, FTL::Config::Key::Page);
  // auto block = readConfigUint(Section::FTL, FTL::Config::Key::Page);
  // auto plane = readConfigUint(Section::FTL, FTL::Config::Key::Page);
  // auto die = readConfigUint(Section::FTL, FTL::Config::Key::Page);
  // auto way = readConfigUint(Section::FTL, FTL::Config::Key::Page);
  // auto channel = readConfigUint(Section::FTL, FTL::Config::Key::Page);
  // auto size = channel * way * die * plane * block * page;
  auto size = 33554432ul; // 8 * 4 * 2 * 2 * 512 * 512

  if (size <= std::numeric_limits<uint16_t>::max()) {
    pHIL = new HIL<uint16_t>(o);
  }
  else if (size <= std::numeric_limits<uint32_t>::max()) {
    pHIL = new HIL<uint32_t>(o);
  }
  else if (size <= std::numeric_limits<uint64_t>::max()) {
    pHIL = new HIL<uint64_t>(o);
  }
  else {
    // Not possible though... (128bit?)
    panic("Too many physical page counts.");
  }
}

Subsystem::~Subsystem() {
  std::visit([](auto&& arg){ delete arg; }, pHIL);
}


}  // namespace SimpleSSD::HIL::NVMe
