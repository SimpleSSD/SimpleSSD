// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "fil/config.hh"

namespace SimpleSSD::FIL {

const char NAME_CHANNEL[] = "Channel";
const char NAME_PACKAGE[] = "Way";
const char NAME_DMA_SPEED[] = "DMASpeed";
const char NAME_DMA_WIDTH[] = "DataWidth";
const char NAME_NVM_MODEL[] = "Model";
const char NAME_SCHEDULER[] = "Scheduler";

/* NAND structure */
const char NAME_NOP[] = "NOP";
const char NAME_DIE[] = "Die";
const char NAME_PLANE[] = "Plane";
const char NAME_BLOCK[] = "Block";
const char NAME_PAGE[] = "Page";
const char NAME_PAGE_SIZE[] = "PageSize";
const char NAME_SPARE_SIZE[] = "SpareSize";
const char NAME_FLASH_TYPE[] = "NANDType";
const char NAME_PAGE_ALLOCATION[] = "PageAllocation";

/* NAND timing */
const char NAME_TADL[] = "tADL";
const char NAME_TCS[] = "tCS";
const char NAME_TDH[] = "tDH";
const char NAME_TDS[] = "tDS";
const char NAME_TRC[] = "tRC";
const char NAME_TRR[] = "tRR";
const char NAME_TWB[] = "tWB";
const char NAME_TWC[] = "tWC";
const char NAME_TWP[] = "tWP";
const char NAME_TBERS[] = "tBERS";
const char NAME_TCBSY[] = "tCBSY";
const char NAME_TDBSY[] = "tDBSY";
const char NAME_TRCBSY[] = "tRCBSY";
const char NAME_TPROG[] = "tPROG";
const char NAME_TR[] = "tR";

/* NAND power */
const char NAME_NAND_VCC[] = "VCC";
const char NAME_NAND_ICC1[] = "ICC1";
const char NAME_NAND_ICC2[] = "ICC2";
const char NAME_NAND_ICC3[] = "ICC3";
const char NAME_NAND_ICC4R[] = "ICC4R";
const char NAME_NAND_ICC4W[] = "ICC4W";
const char NAME_NAND_ICC5[] = "ICC5";
const char NAME_NAND_ISB[] = "ISB";

Config::Config() {
  channel = 8;
  package = 4;
  nvmModel = NVMType::PAL;
  scheduler = SchedulerType::Noop;

  nandStructure.type = NANDType::MLC;
  nandStructure.nop = 1;
  nandStructure.pageAllocation[Level1] = PageAllocation::Channel;
  nandStructure.pageAllocation[Level2] = PageAllocation::Way;
  nandStructure.pageAllocation[Level3] = PageAllocation::Die;
  nandStructure.pageAllocation[Level4] = PageAllocation::Plane;
  nandStructure.die = 1;
  nandStructure.plane = 2;
  nandStructure.block = 512;
  nandStructure.page = 512;
  nandStructure.pageSize = 16384;
  nandStructure.spareSize = 1216;
  nandStructure.dmaSpeed = 400;
  nandStructure.dmaWidth = 8;

  nandTiming.tADL = 70000;
  nandTiming.tCS = 20000;
  nandTiming.tDH = 280;
  nandTiming.tDS = 280;
  nandTiming.tRC = 5000;
  nandTiming.tRR = 20000;
  nandTiming.tWB = 100000;
  nandTiming.tWC = 25000;
  nandTiming.tWP = 11000;
  nandTiming.tBERS = 5'000'000'000;
  nandTiming.tCBSY = 35'000'000;
  nandTiming.tDBSY = 500000;
  nandTiming.tRCBSY = 3'000'000;
  nandTiming.tPROG[Level1] = 1'250'000'000;
  nandTiming.tPROG[Level2] = 3'000'000'000;
  nandTiming.tPROG[Level3] = 0;
  nandTiming.tR[Level1] = 65'000'000;
  nandTiming.tR[Level2] = 110'000'000;
  nandTiming.tR[Level3] = 0;

  nandPower.pVCC = 3300;
  nandPower.current.pICC1 = 25000;
  nandPower.current.pICC2 = 25000;
  nandPower.current.pICC3 = 25000;
  nandPower.current.pICC4R = 10000;
  nandPower.current.pICC4W = 10000;
  nandPower.current.pICC5 = 5000;
  nandPower.current.pISB = 30;
}

void Config::loadNANDStructure(pugi::xml_node &section) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    auto name = node.attribute("name").value();

    LOAD_NAME_UINT_TYPE(node, NAME_NOP, uint8_t, nandStructure.nop);
    LOAD_NAME_UINT_TYPE(node, NAME_DIE, uint32_t, nandStructure.die);
    LOAD_NAME_UINT_TYPE(node, NAME_PLANE, uint32_t, nandStructure.plane);
    LOAD_NAME_UINT_TYPE(node, NAME_BLOCK, uint32_t, nandStructure.block);
    LOAD_NAME_UINT_TYPE(node, NAME_PAGE, uint32_t, nandStructure.page);
    LOAD_NAME_UINT_TYPE(node, NAME_PAGE_SIZE, uint32_t, nandStructure.pageSize);
    LOAD_NAME_UINT_TYPE(node, NAME_DMA_SPEED, uint32_t, nandStructure.dmaSpeed);
    LOAD_NAME_UINT_TYPE(node, NAME_DMA_WIDTH, uint32_t, nandStructure.dmaWidth);
    LOAD_NAME_UINT_TYPE(node, NAME_SPARE_SIZE, uint32_t,
                        nandStructure.spareSize);
    LOAD_NAME_UINT_TYPE(node, NAME_FLASH_TYPE, NANDType, nandStructure.type);
    LOAD_NAME_STRING(node, NAME_PAGE_ALLOCATION, _pageAllocation);

    if (strcmp(name, "timing") == 0 && isSection(node)) {
      loadNANDTiming(node);
    }
    if (strcmp(name, "power") == 0 && isSection(node)) {
      loadNANDPower(node);
    }
  }
}

void Config::loadNANDTiming(pugi::xml_node &section) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_UINT_TYPE(node, NAME_TADL, uint32_t, nandTiming.tADL);
    LOAD_NAME_UINT_TYPE(node, NAME_TCS, uint32_t, nandTiming.tCS);
    LOAD_NAME_UINT_TYPE(node, NAME_TDH, uint32_t, nandTiming.tDH);
    LOAD_NAME_UINT_TYPE(node, NAME_TDS, uint32_t, nandTiming.tDS);
    LOAD_NAME_UINT_TYPE(node, NAME_TRC, uint32_t, nandTiming.tRC);
    LOAD_NAME_UINT_TYPE(node, NAME_TRR, uint32_t, nandTiming.tRR);
    LOAD_NAME_UINT_TYPE(node, NAME_TWB, uint32_t, nandTiming.tWB);
    LOAD_NAME_UINT_TYPE(node, NAME_TWC, uint32_t, nandTiming.tWC);
    LOAD_NAME_UINT_TYPE(node, NAME_TWP, uint32_t, nandTiming.tWP);
    LOAD_NAME_UINT_TYPE(node, NAME_TBERS, uint32_t, nandTiming.tBERS);
    LOAD_NAME_UINT_TYPE(node, NAME_TCBSY, uint32_t, nandTiming.tCBSY);
    LOAD_NAME_UINT_TYPE(node, NAME_TDBSY, uint32_t, nandTiming.tDBSY);
    LOAD_NAME_UINT_TYPE(node, NAME_TRCBSY, uint32_t, nandTiming.tRCBSY);

    switch (strtoul(node.attribute("level").value(), nullptr, 10)) {
      case Level1:
        LOAD_NAME_UINT(node, NAME_TPROG, nandTiming.tPROG[Level1]);
        LOAD_NAME_UINT(node, NAME_TR, nandTiming.tR[Level1]);

        break;
      case Level2:
        LOAD_NAME_UINT(node, NAME_TPROG, nandTiming.tPROG[Level2]);
        LOAD_NAME_UINT(node, NAME_TR, nandTiming.tR[Level2]);

        break;
      case Level3:
        LOAD_NAME_UINT(node, NAME_TPROG, nandTiming.tPROG[Level3]);
        LOAD_NAME_UINT(node, NAME_TR, nandTiming.tR[Level3]);

        break;
    }
  }
}

void Config::loadNANDPower(pugi::xml_node &section) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_UINT(node, NAME_NAND_VCC, nandPower.pVCC);
    LOAD_NAME_UINT(node, NAME_NAND_ICC1, nandPower.current.pICC1);
    LOAD_NAME_UINT(node, NAME_NAND_ICC2, nandPower.current.pICC2);
    LOAD_NAME_UINT(node, NAME_NAND_ICC3, nandPower.current.pICC3);
    LOAD_NAME_UINT(node, NAME_NAND_ICC4R, nandPower.current.pICC4R);
    LOAD_NAME_UINT(node, NAME_NAND_ICC4W, nandPower.current.pICC4W);
    LOAD_NAME_UINT(node, NAME_NAND_ICC5, nandPower.current.pICC5);
    LOAD_NAME_UINT(node, NAME_NAND_ISB, nandPower.current.pISB);
  }
}

void Config::storeNANDStructure(pugi::xml_node &section) {
  pugi::xml_node node;

  STORE_NAME_UINT(section, NAME_NOP, nandStructure.nop);
  STORE_NAME_UINT(section, NAME_DIE, nandStructure.die);
  STORE_NAME_UINT(section, NAME_PLANE, nandStructure.plane);
  STORE_NAME_UINT(section, NAME_BLOCK, nandStructure.block);
  STORE_NAME_UINT(section, NAME_PAGE, nandStructure.page);
  STORE_NAME_UINT(section, NAME_PAGE_SIZE, nandStructure.pageSize);
  STORE_NAME_UINT(section, NAME_SPARE_SIZE, nandStructure.spareSize);
  STORE_NAME_UINT(section, NAME_DMA_SPEED, nandStructure.dmaSpeed);
  STORE_NAME_UINT(section, NAME_DMA_WIDTH, nandStructure.dmaWidth);
  STORE_NAME_UINT(section, NAME_FLASH_TYPE, nandStructure.type);
  STORE_NAME_STRING(section, NAME_PAGE_ALLOCATION, _pageAllocation);

  STORE_SECTION(section, "timing", node);
  storeNANDTiming(node);

  STORE_SECTION(section, "power", node);
  storeNANDPower(node);
}

void Config::storeNANDTiming(pugi::xml_node &section) {
  STORE_NAME_UINT(section, NAME_TADL, nandTiming.tADL);
  STORE_NAME_UINT(section, NAME_TCS, nandTiming.tCS);
  STORE_NAME_UINT(section, NAME_TDH, nandTiming.tDH);
  STORE_NAME_UINT(section, NAME_TDS, nandTiming.tDS);
  STORE_NAME_UINT(section, NAME_TRC, nandTiming.tRC);
  STORE_NAME_UINT(section, NAME_TRR, nandTiming.tRR);
  STORE_NAME_UINT(section, NAME_TWB, nandTiming.tWB);
  STORE_NAME_UINT(section, NAME_TWC, nandTiming.tWC);
  STORE_NAME_UINT(section, NAME_TWP, nandTiming.tWP);
  STORE_NAME_UINT(section, NAME_TBERS, nandTiming.tBERS);
  STORE_NAME_UINT(section, NAME_TCBSY, nandTiming.tCBSY);
  STORE_NAME_UINT(section, NAME_TDBSY, nandTiming.tDBSY);
  STORE_NAME_UINT(section, NAME_TRCBSY, nandTiming.tRCBSY);

  for (uint8_t i = Level1; i <= Level3; i++) {
    auto _child = section.append_child(CONFIG_KEY_NAME);

    if (_child) {
      _child.append_attribute(CONFIG_ATTRIBUTE).set_value(NAME_TPROG);
      _child.append_attribute("level").set_value(i);
      _child.text().set(nandTiming.tPROG[i]);
    }

    _child = section.append_child(CONFIG_KEY_NAME);

    if (_child) {
      _child.append_attribute(CONFIG_ATTRIBUTE).set_value(NAME_TR);
      _child.append_attribute("level").set_value(i);
      _child.text().set(nandTiming.tR[i]);
    }
  }
}

void Config::storeNANDPower(pugi::xml_node &section) {
  STORE_NAME_UINT(section, NAME_NAND_VCC, nandPower.pVCC);
  STORE_NAME_UINT(section, NAME_NAND_ICC1, nandPower.current.pICC1);
  STORE_NAME_UINT(section, NAME_NAND_ICC2, nandPower.current.pICC2);
  STORE_NAME_UINT(section, NAME_NAND_ICC3, nandPower.current.pICC3);
  STORE_NAME_UINT(section, NAME_NAND_ICC4R, nandPower.current.pICC4R);
  STORE_NAME_UINT(section, NAME_NAND_ICC4W, nandPower.current.pICC4W);
  STORE_NAME_UINT(section, NAME_NAND_ICC5, nandPower.current.pICC5);
  STORE_NAME_UINT(section, NAME_NAND_ISB, nandPower.current.pISB);
}

void Config::loadFrom(pugi::xml_node &section) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    auto name = node.attribute("name").value();

    LOAD_NAME_UINT_TYPE(node, NAME_CHANNEL, uint32_t, channel);
    LOAD_NAME_UINT_TYPE(node, NAME_PACKAGE, uint32_t, package);
    LOAD_NAME_UINT_TYPE(node, NAME_NVM_MODEL, NVMType, nvmModel);
    LOAD_NAME_UINT_TYPE(node, NAME_SCHEDULER, SchedulerType, scheduler);

    if (strcmp(name, "nand") == 0 && isSection(node)) {
      loadNANDStructure(node);
    }
  }
}

void Config::storeTo(pugi::xml_node &section) {
  pugi::xml_node node;

  // Re-generate page allocation string
  _pageAllocation.clear();
  _pageAllocation.resize(4);

  for (int i = 0; i < 4; i++) {
    switch (nandStructure.pageAllocation[i]) {
      case PageAllocation::Channel:
        _pageAllocation[i] = 'C';
        break;
      case PageAllocation::Way:
        _pageAllocation[i] = 'W';
        break;
      case PageAllocation::Die:
        _pageAllocation[i] = 'D';
        break;
      case PageAllocation::Plane:
        _pageAllocation[i] = 'P';
        break;
      default:
        _pageAllocation[i] = '?';
        panic_if(true, "Unexpected page allocation.");
        break;
    }
  }

  STORE_NAME_UINT(section, NAME_CHANNEL, channel);
  STORE_NAME_UINT(section, NAME_PACKAGE, package);
  STORE_NAME_UINT(section, NAME_NVM_MODEL, nvmModel);
  STORE_NAME_UINT(section, NAME_SCHEDULER, scheduler);

  STORE_SECTION(section, "nand", node);
  storeNANDStructure(node);
}

void Config::update() {
  panic_if(nandStructure.dmaWidth & 0x07, "dmaWidth should be multiple of 8.");

  // Parse page allocation setting
  int i = 0;
  uint8_t check = PageAllocation::None;
  bool fail = false;

  for (auto &iter : _pageAllocation) {
    if ((iter == 'C') | (iter == 'c')) {
      if (check & PageAllocation::Channel) {
        fail = true;
      }

      nandStructure.pageAllocation[i++] = PageAllocation::Channel;
      check |= PageAllocation::Channel;
    }
    else if ((iter == 'W') | (iter == 'w')) {
      if (check & PageAllocation::Way) {
        fail = true;
      }

      nandStructure.pageAllocation[i++] = PageAllocation::Way;
      check |= PageAllocation::Way;
    }
    else if ((iter == 'D') | (iter == 'd')) {
      if (check & PageAllocation::Die) {
        fail = true;
      }

      nandStructure.pageAllocation[i++] = PageAllocation::Die;
      check |= PageAllocation::Die;
    }
    else if ((iter == 'P') | (iter == 'p')) {
      if (check & PageAllocation::Plane) {
        fail = true;
      }

      nandStructure.pageAllocation[i++] = PageAllocation::Plane;
      check |= PageAllocation::Plane;
    }

    if (i == 4) {
      break;
    }
  }

  panic_if(check != PageAllocation::All || fail,
           "Invalid page allocation string");
}

uint64_t Config::readUint(uint32_t idx) {
  uint64_t ret = 0;

  switch (idx) {
    case Channel:
      ret = channel;
      break;
    case Way:
      ret = package;
      break;
    case Model:
      ret = (uint64_t)nvmModel;
      break;
    case Scheduler:
      ret = (uint64_t)scheduler;
      break;
  }

  return ret;
}

bool Config::writeUint(uint32_t idx, uint64_t value) {
  bool ret = true;

  switch (idx) {
    case Channel:
      channel = (uint32_t)value;
      break;
    case Way:
      package = (uint32_t)value;
      break;
    case Model:
      nvmModel = (NVMType)value;
      break;
    case Scheduler:
      scheduler = (SchedulerType)value;
      break;
    default:
      ret = false;
      break;
  }

  return ret;
}

Config::NANDStructure *Config::getNANDStructure() {
  return &nandStructure;
}

Config::NANDTiming *Config::getNANDTiming() {
  return &nandTiming;
}

Config::NANDPower *Config::getNANDPower() {
  return &nandPower;
}

}  // namespace SimpleSSD::FIL
