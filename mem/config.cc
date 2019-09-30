// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/config.hh"

namespace SimpleSSD::Memory {

const char NAME_MODEL[] = "Model";
const char NAME_LINE_SIZE[] = "LineSize";
const char NAME_SIZE[] = "Size";
const char NAME_LATENCY[] = "Latency";
const char NAME_CHANNEL[] = "Channel";
const char NAME_RANK[] = "Rank";
const char NAME_BANK[] = "Bank";
const char NAME_CHIP[] = "Chip";
const char NAME_BUS_WIDTH[] = "BusWidth";
const char NAME_BURST[] = "BurstLength";
const char NAME_CHIP_SIZE[] = "ChipSize";
const char NAME_TCK[] = "tCK";
const char NAME_TRCD[] = "tRCD";
const char NAME_TCL[] = "tCL";
const char NAME_TRP[] = "tRP";
const char NAME_TRAS[] = "tRAS";
const char NAME_TWR[] = "tWR";
const char NAME_TRTP[] = "tRTP";
const char NAME_TBURST[] = "tBURST";
const char NAME_TCCD_L[] = "tCCD_L";
const char NAME_TRFC[] = "tRFC";
const char NAME_TREFI[] = "tREFI";
const char NAME_TWTR[] = "tWTR";
const char NAME_TRTW[] = "tRTW";
const char NAME_TCS[] = "tCS";
const char NAME_TRRD[] = "tRRD";
const char NAME_TRRD_L[] = "tRRD_L";
const char NAME_TXAW[] = "tXAW";
const char NAME_TXP[] = "tXP";
const char NAME_TXPDLL[] = "tXPDLL";
const char NAME_TXS[] = "tXS";
const char NAME_TXSDLL[] = "tXSDLL";
const char NAME_IDD0_0[] = "IDD0_0";
const char NAME_IDD0_1[] = "IDD0_1";
const char NAME_IDD2P0_0[] = "IDD2P0_0";
const char NAME_IDD2P0_1[] = "IDD2P0_1";
const char NAME_IDD2P1_0[] = "IDD2P1_0";
const char NAME_IDD2P1_1[] = "IDD2P1_1";
const char NAME_IDD2N_0[] = "IDD2N_0";
const char NAME_IDD2N_1[] = "IDD2N_1";
const char NAME_IDD3P0_0[] = "IDD3P0_0";
const char NAME_IDD3P0_1[] = "IDD3P0_1";
const char NAME_IDD3P1_0[] = "IDD3P1_0";
const char NAME_IDD3P1_1[] = "IDD3P1_1";
const char NAME_IDD3N_0[] = "IDD3N_0";
const char NAME_IDD3N_1[] = "IDD3N_1";
const char NAME_IDD4R_0[] = "IDD4R_0";
const char NAME_IDD4R_1[] = "IDD4R_1";
const char NAME_IDD4W_0[] = "IDD4W_0";
const char NAME_IDD4W_1[] = "IDD4W_1";
const char NAME_IDD5_0[] = "IDD5_0";
const char NAME_IDD5_1[] = "IDD5_1";
const char NAME_IDD6_0[] = "IDD6_0";
const char NAME_IDD6_1[] = "IDD6_1";
const char NAME_VDD_0[] = "VDD_0";
const char NAME_VDD_1[] = "VDD_1";

Config::Config() {
  /* 32MB SRAM */
  sram.lineSize = 64;
  sram.size = 33554432;
  sram.latency = 20;

  /* LPDDR3-1600 4Gbit 1x32 */
  dramModel = Model::Simple;

  dram.channel = 1;
  dram.rank = 1;
  dram.bank = 8;
  dram.chip = 1;
  dram.chipSize = 536870912;
  dram.width = 32;
  dram.burst = 8;
  dram.activationLimit = 4;
  dram.useDLL = false;
  dram.pageSize = 4096;

  timing.tCK = 1250;
  timing.tRCD = 18000;
  timing.tCL = 15000;
  timing.tRP = 18000;
  timing.tRAS = 42000;
  timing.tWR = 15000;
  timing.tRTP = 7500;
  timing.tBURST = 5000;
  timing.tCCD_L = 0;
  timing.tRFC = 130000;
  timing.tREFI = 3900;
  timing.tWTR = 7500;
  timing.tRTW = 2500;
  timing.tCS = 2500;
  timing.tRRD = 10000;
  timing.tRRD_L = 0;
  timing.tXAW = 50000;
  timing.tXP = 0;
  timing.tXPDLL = 0;
  timing.tXS = 0;
  timing.tXSDLL = 0;

  power.pIDD0[0] = 8.f;
  power.pIDD0[1] = 60.f;
  power.pIDD2P0[0] = 0.f;
  power.pIDD2P0[1] = 0.f;
  power.pIDD2P1[0] = 0.8f;
  power.pIDD2P1[1] = 1.8f;
  power.pIDD2N[0] = 0.8f;
  power.pIDD2N[1] = 26.f;
  power.pIDD3P0[0] = 0.f;
  power.pIDD3P0[1] = 0.f;
  power.pIDD3P1[0] = 1.4f;
  power.pIDD3P1[1] = 11.f;
  power.pIDD3N[0] = 2.f;
  power.pIDD3N[1] = 34.f;
  power.pIDD4R[0] = 2.f;
  power.pIDD4R[1] = 230.f;
  power.pIDD4W[0] = 2.f;
  power.pIDD4W[1] = 190.f;
  power.pIDD5[0] = 28.f;
  power.pIDD5[1] = 150.f;
  power.pIDD6[0] = 0.5f;
  power.pIDD6[1] = 1.8f;
  power.pVDD[0] = 1.8f;
  power.pVDD[1] = 1.2f;
}

Config::~Config() {}

void Config::loadSRAM(pugi::xml_node &section, Config::SRAMStructure *param) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_UINT_TYPE(node, NAME_LINE_SIZE, uint16_t, param->lineSize);
    LOAD_NAME_UINT(node, NAME_SIZE, param->size);
    LOAD_NAME_UINT(node, NAME_LATENCY, param->latency);
  }
}

void Config::loadDRAMStructure(pugi::xml_node &section,
                               Config::DRAMStructure *param) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_UINT_TYPE(node, NAME_CHANNEL, uint8_t, param->channel);
    LOAD_NAME_UINT_TYPE(node, NAME_RANK, uint8_t, param->rank);
    LOAD_NAME_UINT_TYPE(node, NAME_BANK, uint8_t, param->bank);
    LOAD_NAME_UINT_TYPE(node, NAME_CHIP, uint8_t, param->chip);
    LOAD_NAME_UINT_TYPE(node, NAME_BUS_WIDTH, uint16_t, param->width);
    LOAD_NAME_UINT_TYPE(node, NAME_BURST, uint16_t, param->burst);
    LOAD_NAME_UINT(node, NAME_CHIP_SIZE, param->chipSize);
  }
}

void Config::loadDRAMTiming(pugi::xml_node &section,
                            Config::DRAMTiming *param) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_UINT_TYPE(node, NAME_TCK, uint32_t, param->tCK);
    LOAD_NAME_UINT_TYPE(node, NAME_TRCD, uint32_t, param->tRCD);
    LOAD_NAME_UINT_TYPE(node, NAME_TCL, uint32_t, param->tCL);
    LOAD_NAME_UINT_TYPE(node, NAME_TRP, uint32_t, param->tRP);
    LOAD_NAME_UINT_TYPE(node, NAME_TRAS, uint32_t, param->tRAS);
    LOAD_NAME_UINT_TYPE(node, NAME_TWR, uint32_t, param->tWR);
    LOAD_NAME_UINT_TYPE(node, NAME_TRTP, uint32_t, param->tRTP);
    LOAD_NAME_UINT_TYPE(node, NAME_TBURST, uint32_t, param->tBURST);
    LOAD_NAME_UINT_TYPE(node, NAME_TCCD_L, uint32_t, param->tCCD_L);
    LOAD_NAME_UINT_TYPE(node, NAME_TRFC, uint32_t, param->tRFC);
    LOAD_NAME_UINT_TYPE(node, NAME_TREFI, uint32_t, param->tREFI);
    LOAD_NAME_UINT_TYPE(node, NAME_TWTR, uint32_t, param->tWTR);
    LOAD_NAME_UINT_TYPE(node, NAME_TRTW, uint32_t, param->tRTW);
    LOAD_NAME_UINT_TYPE(node, NAME_TCS, uint32_t, param->tCS);
    LOAD_NAME_UINT_TYPE(node, NAME_TRRD, uint32_t, param->tRRD);
    LOAD_NAME_UINT_TYPE(node, NAME_TRRD_L, uint32_t, param->tRRD_L);
    LOAD_NAME_UINT_TYPE(node, NAME_TXAW, uint32_t, param->tXAW);
    LOAD_NAME_UINT_TYPE(node, NAME_TXP, uint32_t, param->tXP);
    LOAD_NAME_UINT_TYPE(node, NAME_TXPDLL, uint32_t, param->tXPDLL);
    LOAD_NAME_UINT_TYPE(node, NAME_TXS, uint32_t, param->tXS);
    LOAD_NAME_UINT_TYPE(node, NAME_TXSDLL, uint32_t, param->tXSDLL);
  }
}

void Config::loadDRAMPower(pugi::xml_node &section, Config::DRAMPower *param) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_FLOAT(node, NAME_IDD0_0, param->pIDD0[0]);
    LOAD_NAME_FLOAT(node, NAME_IDD0_1, param->pIDD0[1]);
    LOAD_NAME_FLOAT(node, NAME_IDD2P0_0, param->pIDD2P0[0]);
    LOAD_NAME_FLOAT(node, NAME_IDD2P0_1, param->pIDD2P0[1]);
    LOAD_NAME_FLOAT(node, NAME_IDD2P1_0, param->pIDD2P1[0]);
    LOAD_NAME_FLOAT(node, NAME_IDD2P1_1, param->pIDD2P1[1]);
    LOAD_NAME_FLOAT(node, NAME_IDD2N_0, param->pIDD2N[0]);
    LOAD_NAME_FLOAT(node, NAME_IDD2N_1, param->pIDD2N[1]);
    LOAD_NAME_FLOAT(node, NAME_IDD3P0_0, param->pIDD3P0[0]);
    LOAD_NAME_FLOAT(node, NAME_IDD3P0_1, param->pIDD3P0[1]);
    LOAD_NAME_FLOAT(node, NAME_IDD3P1_0, param->pIDD3P1[0]);
    LOAD_NAME_FLOAT(node, NAME_IDD3P1_1, param->pIDD3P1[1]);
    LOAD_NAME_FLOAT(node, NAME_IDD3N_0, param->pIDD3N[0]);
    LOAD_NAME_FLOAT(node, NAME_IDD3N_1, param->pIDD3N[1]);
    LOAD_NAME_FLOAT(node, NAME_IDD4R_0, param->pIDD4R[0]);
    LOAD_NAME_FLOAT(node, NAME_IDD4R_1, param->pIDD4R[1]);
    LOAD_NAME_FLOAT(node, NAME_IDD4W_0, param->pIDD4W[0]);
    LOAD_NAME_FLOAT(node, NAME_IDD4W_1, param->pIDD4W[1]);
    LOAD_NAME_FLOAT(node, NAME_IDD5_0, param->pIDD5[0]);
    LOAD_NAME_FLOAT(node, NAME_IDD5_1, param->pIDD5[1]);
    LOAD_NAME_FLOAT(node, NAME_IDD6_0, param->pIDD6[0]);
    LOAD_NAME_FLOAT(node, NAME_IDD6_1, param->pIDD6[1]);
    LOAD_NAME_FLOAT(node, NAME_VDD_0, param->pVDD[0]);
    LOAD_NAME_FLOAT(node, NAME_VDD_1, param->pVDD[1]);
  }
}

void Config::storeSRAM(pugi::xml_node &section, Config::SRAMStructure *param) {
  STORE_NAME_UINT(section, NAME_LINE_SIZE, param->lineSize);
  STORE_NAME_UINT(section, NAME_SIZE, param->size);
  STORE_NAME_UINT(section, NAME_LATENCY, param->latency);
}

void Config::storeDRAMStructure(pugi::xml_node &section,
                                Config::DRAMStructure *param) {
  STORE_NAME_UINT(section, NAME_CHANNEL, param->channel);
  STORE_NAME_UINT(section, NAME_RANK, param->rank);
  STORE_NAME_UINT(section, NAME_BANK, param->bank);
  STORE_NAME_UINT(section, NAME_CHIP, param->chip);
  STORE_NAME_UINT(section, NAME_BUS_WIDTH, param->width);
  STORE_NAME_UINT(section, NAME_BURST, param->burst);
  STORE_NAME_UINT(section, NAME_CHIP_SIZE, param->chipSize);
}

void Config::storeDRAMTiming(pugi::xml_node &section,
                             Config::DRAMTiming *param) {
  STORE_NAME_UINT(section, NAME_TCK, param->tCK);
  STORE_NAME_UINT(section, NAME_TRCD, param->tRCD);
  STORE_NAME_UINT(section, NAME_TCL, param->tCL);
  STORE_NAME_UINT(section, NAME_TRP, param->tRP);
  STORE_NAME_UINT(section, NAME_TRAS, param->tRAS);
  STORE_NAME_UINT(section, NAME_TWR, param->tWR);
  STORE_NAME_UINT(section, NAME_TRTP, param->tRTP);
  STORE_NAME_UINT(section, NAME_TBURST, param->tBURST);
  STORE_NAME_UINT(section, NAME_TCCD_L, param->tCCD_L);
  STORE_NAME_UINT(section, NAME_TRFC, param->tRFC);
  STORE_NAME_UINT(section, NAME_TREFI, param->tREFI);
  STORE_NAME_UINT(section, NAME_TWTR, param->tWTR);
  STORE_NAME_UINT(section, NAME_TRTW, param->tRTW);
  STORE_NAME_UINT(section, NAME_TCS, param->tCS);
  STORE_NAME_UINT(section, NAME_TRRD, param->tRRD);
  STORE_NAME_UINT(section, NAME_TRRD_L, param->tRRD_L);
  STORE_NAME_UINT(section, NAME_TXAW, param->tXAW);
  STORE_NAME_UINT(section, NAME_TXP, param->tXP);
  STORE_NAME_UINT(section, NAME_TXPDLL, param->tXPDLL);
  STORE_NAME_UINT(section, NAME_TXS, param->tXS);
  STORE_NAME_UINT(section, NAME_TXSDLL, param->tXSDLL);
}

void Config::storeDRAMPower(pugi::xml_node &section, Config::DRAMPower *param) {
  STORE_NAME_FLOAT(section, NAME_IDD0_0, param->pIDD0[0]);
  STORE_NAME_FLOAT(section, NAME_IDD0_1, param->pIDD0[1]);
  STORE_NAME_FLOAT(section, NAME_IDD2P0_0, param->pIDD2P0[0]);
  STORE_NAME_FLOAT(section, NAME_IDD2P0_1, param->pIDD2P0[1]);
  STORE_NAME_FLOAT(section, NAME_IDD2P1_0, param->pIDD2P1[0]);
  STORE_NAME_FLOAT(section, NAME_IDD2P1_1, param->pIDD2P1[1]);
  STORE_NAME_FLOAT(section, NAME_IDD2N_0, param->pIDD2N[0]);
  STORE_NAME_FLOAT(section, NAME_IDD2N_1, param->pIDD2N[1]);
  STORE_NAME_FLOAT(section, NAME_IDD3P0_0, param->pIDD3P0[0]);
  STORE_NAME_FLOAT(section, NAME_IDD3P0_1, param->pIDD3P0[1]);
  STORE_NAME_FLOAT(section, NAME_IDD3P1_0, param->pIDD3P1[0]);
  STORE_NAME_FLOAT(section, NAME_IDD3P1_1, param->pIDD3P1[1]);
  STORE_NAME_FLOAT(section, NAME_IDD3N_0, param->pIDD3N[0]);
  STORE_NAME_FLOAT(section, NAME_IDD3N_1, param->pIDD3N[1]);
  STORE_NAME_FLOAT(section, NAME_IDD4R_0, param->pIDD4R[0]);
  STORE_NAME_FLOAT(section, NAME_IDD4R_1, param->pIDD4R[1]);
  STORE_NAME_FLOAT(section, NAME_IDD4W_0, param->pIDD4W[0]);
  STORE_NAME_FLOAT(section, NAME_IDD4W_1, param->pIDD4W[1]);
  STORE_NAME_FLOAT(section, NAME_IDD5_0, param->pIDD5[0]);
  STORE_NAME_FLOAT(section, NAME_IDD5_1, param->pIDD5[1]);
  STORE_NAME_FLOAT(section, NAME_IDD6_0, param->pIDD6[0]);
  STORE_NAME_FLOAT(section, NAME_IDD6_1, param->pIDD6[1]);
  STORE_NAME_FLOAT(section, NAME_VDD_0, param->pVDD[0]);
  STORE_NAME_FLOAT(section, NAME_VDD_1, param->pVDD[1]);
}

void Config::loadFrom(pugi::xml_node &section) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    auto name = node.attribute("name").value();

    if (strcmp(name, "sram") == 0 && isSection(node)) {
      loadSRAM(node, &sram);
    }
    else if (strcmp(name, "dram") == 0) {
      for (auto node2 = node.first_child(); node2;
           node2 = node2.next_sibling()) {
        auto name2 = node2.attribute("name").value();

        if (strcmp(name2, "struct") == 0 && isSection(node2)) {
          loadDRAMStructure(node2, &dram);
        }
        else if (strcmp(name2, "timing") == 0 && isSection(node2)) {
          loadDRAMTiming(node2, &timing);
        }
        else if (strcmp(name2, "power") == 0 && isSection(node2)) {
          loadDRAMPower(node2, &power);
        }

        LOAD_NAME_UINT_TYPE(node2, NAME_MODEL, Model, dramModel);
      }
    }
  }
}

void Config::storeTo(pugi::xml_node &section) {
  pugi::xml_node node, node2;

  STORE_SECTION(section, "sram", node);
  storeSRAM(node, &sram);

  STORE_SECTION(section, "dram", node);
  STORE_NAME_UINT(node, NAME_MODEL, dramModel);

  STORE_SECTION(node, "struct", node2);
  storeDRAMStructure(node2, &dram);

  STORE_SECTION(node, "timing", node2);
  storeDRAMTiming(node2, &timing);

  STORE_SECTION(node, "power", node2);
  storeDRAMPower(node2, &power);
}

void Config::update() {
  // Validate SRAM
  panic_if(sram.lineSize == 0, "Invalid line size");
  panic_if(sram.latency == 0, "Invalid latency");

  if (sram.size > 0) {
    panic_if(sram.lineSize % sram.size != 0, "Size not aligned");
  }
}

uint64_t Config::readUint(uint32_t idx) {
  switch (idx) {
    case Key::DRAMModel:
      return dramModel;
  }

  return 0;
}

bool Config::writeUint(uint32_t idx, uint64_t value) {
  bool ret = true;

  switch (idx) {
    case Key::DRAMModel:
      dramModel = (Model)value;
      break;
    default:
      ret = false;
      break;
  }

  return ret;
}

Config::SRAMStructure *Config::getSRAM() {
  return &sram;
}

Config::DRAMStructure *Config::getDRAM() {
  return &dram;
}

Config::DRAMTiming *Config::getDRAMTiming() {
  return &timing;
}

Config::DRAMPower *Config::getDRAMPower() {
  return &power;
}

}  // namespace SimpleSSD::Memory
