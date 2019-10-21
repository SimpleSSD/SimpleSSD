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
const char NAME_TCCD_L_WR[] = "tCCD_L_WR";
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
const char NAME_WRITE_BUFFER_SIZE[] = "WriteBufferSize";
const char NAME_READ_BUFFER_SIZE[] = "ReadBufferSize";
const char NAME_FORCE_WRITE_THRESHOLD[] = "ForceWriteThreshold";
const char NAME_START_WRITE_THRESHOLD[] = "WriteThreshold";
const char NAME_MIN_WRITE_BURST[] = "MinWriteBurst";
const char NAME_MEMORY_SCHEDULING[] = "Scheduling";
const char NAME_ADDRESS_MAPPING[] = "Mapping";
const char NAME_PAGE_POLICY[] = "PagePolicy";
const char NAME_MAX_ACCESS_PER_ROW[] = "MaxAccessPerRow";
const char NAME_FRONTEND_LATENCY[] = "FrontendLatency";
const char NAME_BACKEND_LATENCY[] = "BackendLatency";
const char NAME_ROW_BUFFER_SIZE[] = "RowbufferSize";
const char NAME_BANK_GROUP[] = "BankGroup";
const char NAME_ENABLE_POWERDOWN[] = "EnablePowerdown";
const char NAME_USE_DLL[] = "DLL";

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

void Config::loadSRAM(pugi::xml_node &section) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_UINT_TYPE(node, NAME_LINE_SIZE, uint16_t, sram.lineSize);
    LOAD_NAME_UINT(node, NAME_SIZE, sram.size);
    LOAD_NAME_UINT(node, NAME_LATENCY, sram.latency);
  }
}

void Config::loadDRAMStructure(pugi::xml_node &section) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_UINT_TYPE(node, NAME_CHANNEL, uint8_t, dram.channel);
    LOAD_NAME_UINT_TYPE(node, NAME_RANK, uint8_t, dram.rank);
    LOAD_NAME_UINT_TYPE(node, NAME_BANK, uint8_t, dram.bank);
    LOAD_NAME_UINT_TYPE(node, NAME_CHIP, uint8_t, dram.chip);
    LOAD_NAME_UINT_TYPE(node, NAME_BUS_WIDTH, uint16_t, dram.width);
    LOAD_NAME_UINT_TYPE(node, NAME_BURST, uint16_t, dram.burst);
    LOAD_NAME_UINT(node, NAME_CHIP_SIZE, dram.chipSize);
  }
}

void Config::loadDRAMTiming(pugi::xml_node &section) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_TIME(node, NAME_TCK, timing.tCK);
    LOAD_NAME_TIME(node, NAME_TRCD, timing.tRCD);
    LOAD_NAME_TIME(node, NAME_TCL, timing.tCL);
    LOAD_NAME_TIME(node, NAME_TRP, timing.tRP);
    LOAD_NAME_TIME(node, NAME_TRAS, timing.tRAS);
    LOAD_NAME_TIME(node, NAME_TWR, timing.tWR);
    LOAD_NAME_TIME(node, NAME_TRTP, timing.tRTP);
    LOAD_NAME_TIME(node, NAME_TBURST, timing.tBURST);
    LOAD_NAME_TIME(node, NAME_TCCD_L, timing.tCCD_L);
    LOAD_NAME_TIME(node, NAME_TCCD_L_WR, timing.tCCD_L_WR);
    LOAD_NAME_TIME(node, NAME_TRFC, timing.tRFC);
    LOAD_NAME_TIME(node, NAME_TREFI, timing.tREFI);
    LOAD_NAME_TIME(node, NAME_TWTR, timing.tWTR);
    LOAD_NAME_TIME(node, NAME_TRTW, timing.tRTW);
    LOAD_NAME_TIME(node, NAME_TCS, timing.tCS);
    LOAD_NAME_TIME(node, NAME_TRRD, timing.tRRD);
    LOAD_NAME_TIME(node, NAME_TRRD_L, timing.tRRD_L);
    LOAD_NAME_TIME(node, NAME_TXAW, timing.tXAW);
    LOAD_NAME_TIME(node, NAME_TXP, timing.tXP);
    LOAD_NAME_TIME(node, NAME_TXPDLL, timing.tXPDLL);
    LOAD_NAME_TIME(node, NAME_TXS, timing.tXS);
    LOAD_NAME_TIME(node, NAME_TXSDLL, timing.tXSDLL);
  }
}

void Config::loadDRAMPower(pugi::xml_node &section) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_FLOAT(node, NAME_IDD0_0, power.pIDD0[0]);
    LOAD_NAME_FLOAT(node, NAME_IDD0_1, power.pIDD0[1]);
    LOAD_NAME_FLOAT(node, NAME_IDD2P0_0, power.pIDD2P0[0]);
    LOAD_NAME_FLOAT(node, NAME_IDD2P0_1, power.pIDD2P0[1]);
    LOAD_NAME_FLOAT(node, NAME_IDD2P1_0, power.pIDD2P1[0]);
    LOAD_NAME_FLOAT(node, NAME_IDD2P1_1, power.pIDD2P1[1]);
    LOAD_NAME_FLOAT(node, NAME_IDD2N_0, power.pIDD2N[0]);
    LOAD_NAME_FLOAT(node, NAME_IDD2N_1, power.pIDD2N[1]);
    LOAD_NAME_FLOAT(node, NAME_IDD3P0_0, power.pIDD3P0[0]);
    LOAD_NAME_FLOAT(node, NAME_IDD3P0_1, power.pIDD3P0[1]);
    LOAD_NAME_FLOAT(node, NAME_IDD3P1_0, power.pIDD3P1[0]);
    LOAD_NAME_FLOAT(node, NAME_IDD3P1_1, power.pIDD3P1[1]);
    LOAD_NAME_FLOAT(node, NAME_IDD3N_0, power.pIDD3N[0]);
    LOAD_NAME_FLOAT(node, NAME_IDD3N_1, power.pIDD3N[1]);
    LOAD_NAME_FLOAT(node, NAME_IDD4R_0, power.pIDD4R[0]);
    LOAD_NAME_FLOAT(node, NAME_IDD4R_1, power.pIDD4R[1]);
    LOAD_NAME_FLOAT(node, NAME_IDD4W_0, power.pIDD4W[0]);
    LOAD_NAME_FLOAT(node, NAME_IDD4W_1, power.pIDD4W[1]);
    LOAD_NAME_FLOAT(node, NAME_IDD5_0, power.pIDD5[0]);
    LOAD_NAME_FLOAT(node, NAME_IDD5_1, power.pIDD5[1]);
    LOAD_NAME_FLOAT(node, NAME_IDD6_0, power.pIDD6[0]);
    LOAD_NAME_FLOAT(node, NAME_IDD6_1, power.pIDD6[1]);
    LOAD_NAME_FLOAT(node, NAME_VDD_0, power.pVDD[0]);
    LOAD_NAME_FLOAT(node, NAME_VDD_1, power.pVDD[1]);
  }
}

void Config::loadTimingDRAM(pugi::xml_node &section) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_UINT(node, NAME_WRITE_BUFFER_SIZE, gem5.writeBufferSize);
    LOAD_NAME_UINT(node, NAME_READ_BUFFER_SIZE, gem5.readBufferSize);
    LOAD_NAME_FLOAT(node, NAME_FORCE_WRITE_THRESHOLD, gem5.forceWriteThreshold);
    LOAD_NAME_FLOAT(node, NAME_START_WRITE_THRESHOLD, gem5.startWriteThreshold);
    LOAD_NAME_UINT(node, NAME_MIN_WRITE_BURST, gem5.minWriteBurst);
    LOAD_NAME_UINT_TYPE(node, NAME_MEMORY_SCHEDULING, MemoryScheduling,
                        gem5.scheduling);
    LOAD_NAME_UINT_TYPE(node, NAME_ADDRESS_MAPPING, AddressMapping,
                        gem5.mapping);
    LOAD_NAME_UINT_TYPE(node, NAME_PAGE_POLICY, PagePolicy, gem5.policy);
    LOAD_NAME_UINT(node, NAME_MAX_ACCESS_PER_ROW, gem5.frontendLatency);
    LOAD_NAME_UINT(node, NAME_FRONTEND_LATENCY, gem5.backendLatency);
    LOAD_NAME_UINT(node, NAME_BACKEND_LATENCY, gem5.maxAccessesPerRow);
    LOAD_NAME_UINT(node, NAME_ROW_BUFFER_SIZE, gem5.rowBufferSize);
    LOAD_NAME_UINT(node, NAME_BANK_GROUP, gem5.bankGroup);
    LOAD_NAME_BOOLEAN(node, NAME_ENABLE_POWERDOWN, gem5.enablePowerdown);
    LOAD_NAME_BOOLEAN(node, NAME_USE_DLL, gem5.useDLL);
  }
}

void Config::storeSRAM(pugi::xml_node &section) {
  STORE_NAME_UINT(section, NAME_LINE_SIZE, sram.lineSize);
  STORE_NAME_UINT(section, NAME_SIZE, sram.size);
  STORE_NAME_UINT(section, NAME_LATENCY, sram.latency);
}

void Config::storeDRAMStructure(pugi::xml_node &section) {
  STORE_NAME_UINT(section, NAME_CHANNEL, dram.channel);
  STORE_NAME_UINT(section, NAME_RANK, dram.rank);
  STORE_NAME_UINT(section, NAME_BANK, dram.bank);
  STORE_NAME_UINT(section, NAME_CHIP, dram.chip);
  STORE_NAME_UINT(section, NAME_BUS_WIDTH, dram.width);
  STORE_NAME_UINT(section, NAME_BURST, dram.burst);
  STORE_NAME_UINT(section, NAME_CHIP_SIZE, dram.chipSize);
}

void Config::storeDRAMTiming(pugi::xml_node &section) {
  STORE_NAME_UINT(section, NAME_TCK, timing.tCK);
  STORE_NAME_UINT(section, NAME_TRCD, timing.tRCD);
  STORE_NAME_UINT(section, NAME_TCL, timing.tCL);
  STORE_NAME_UINT(section, NAME_TRP, timing.tRP);
  STORE_NAME_UINT(section, NAME_TRAS, timing.tRAS);
  STORE_NAME_UINT(section, NAME_TWR, timing.tWR);
  STORE_NAME_UINT(section, NAME_TRTP, timing.tRTP);
  STORE_NAME_UINT(section, NAME_TBURST, timing.tBURST);
  STORE_NAME_UINT(section, NAME_TCCD_L, timing.tCCD_L);
  STORE_NAME_UINT(section, NAME_TCCD_L_WR, timing.tCCD_L_WR);
  STORE_NAME_UINT(section, NAME_TRFC, timing.tRFC);
  STORE_NAME_UINT(section, NAME_TREFI, timing.tREFI);
  STORE_NAME_UINT(section, NAME_TWTR, timing.tWTR);
  STORE_NAME_UINT(section, NAME_TRTW, timing.tRTW);
  STORE_NAME_UINT(section, NAME_TCS, timing.tCS);
  STORE_NAME_UINT(section, NAME_TRRD, timing.tRRD);
  STORE_NAME_UINT(section, NAME_TRRD_L, timing.tRRD_L);
  STORE_NAME_UINT(section, NAME_TXAW, timing.tXAW);
  STORE_NAME_UINT(section, NAME_TXP, timing.tXP);
  STORE_NAME_UINT(section, NAME_TXPDLL, timing.tXPDLL);
  STORE_NAME_UINT(section, NAME_TXS, timing.tXS);
  STORE_NAME_UINT(section, NAME_TXSDLL, timing.tXSDLL);
}

void Config::storeDRAMPower(pugi::xml_node &section) {
  STORE_NAME_FLOAT(section, NAME_IDD0_0, power.pIDD0[0]);
  STORE_NAME_FLOAT(section, NAME_IDD0_1, power.pIDD0[1]);
  STORE_NAME_FLOAT(section, NAME_IDD2P0_0, power.pIDD2P0[0]);
  STORE_NAME_FLOAT(section, NAME_IDD2P0_1, power.pIDD2P0[1]);
  STORE_NAME_FLOAT(section, NAME_IDD2P1_0, power.pIDD2P1[0]);
  STORE_NAME_FLOAT(section, NAME_IDD2P1_1, power.pIDD2P1[1]);
  STORE_NAME_FLOAT(section, NAME_IDD2N_0, power.pIDD2N[0]);
  STORE_NAME_FLOAT(section, NAME_IDD2N_1, power.pIDD2N[1]);
  STORE_NAME_FLOAT(section, NAME_IDD3P0_0, power.pIDD3P0[0]);
  STORE_NAME_FLOAT(section, NAME_IDD3P0_1, power.pIDD3P0[1]);
  STORE_NAME_FLOAT(section, NAME_IDD3P1_0, power.pIDD3P1[0]);
  STORE_NAME_FLOAT(section, NAME_IDD3P1_1, power.pIDD3P1[1]);
  STORE_NAME_FLOAT(section, NAME_IDD3N_0, power.pIDD3N[0]);
  STORE_NAME_FLOAT(section, NAME_IDD3N_1, power.pIDD3N[1]);
  STORE_NAME_FLOAT(section, NAME_IDD4R_0, power.pIDD4R[0]);
  STORE_NAME_FLOAT(section, NAME_IDD4R_1, power.pIDD4R[1]);
  STORE_NAME_FLOAT(section, NAME_IDD4W_0, power.pIDD4W[0]);
  STORE_NAME_FLOAT(section, NAME_IDD4W_1, power.pIDD4W[1]);
  STORE_NAME_FLOAT(section, NAME_IDD5_0, power.pIDD5[0]);
  STORE_NAME_FLOAT(section, NAME_IDD5_1, power.pIDD5[1]);
  STORE_NAME_FLOAT(section, NAME_IDD6_0, power.pIDD6[0]);
  STORE_NAME_FLOAT(section, NAME_IDD6_1, power.pIDD6[1]);
  STORE_NAME_FLOAT(section, NAME_VDD_0, power.pVDD[0]);
  STORE_NAME_FLOAT(section, NAME_VDD_1, power.pVDD[1]);
}

void Config::storeTimingDRAM(pugi::xml_node &section) {
  STORE_NAME_UINT(section, NAME_WRITE_BUFFER_SIZE, gem5.writeBufferSize);
  STORE_NAME_UINT(section, NAME_READ_BUFFER_SIZE, gem5.readBufferSize);
  STORE_NAME_FLOAT(section, NAME_FORCE_WRITE_THRESHOLD,
                   gem5.forceWriteThreshold);
  STORE_NAME_FLOAT(section, NAME_START_WRITE_THRESHOLD,
                   gem5.startWriteThreshold);
  STORE_NAME_UINT(section, NAME_MIN_WRITE_BURST, gem5.minWriteBurst);
  STORE_NAME_UINT(section, NAME_MEMORY_SCHEDULING, gem5.scheduling);
  STORE_NAME_UINT(section, NAME_ADDRESS_MAPPING, gem5.mapping);
  STORE_NAME_UINT(section, NAME_PAGE_POLICY, gem5.policy);
  STORE_NAME_UINT(section, NAME_MAX_ACCESS_PER_ROW, gem5.frontendLatency);
  STORE_NAME_UINT(section, NAME_FRONTEND_LATENCY, gem5.backendLatency);
  STORE_NAME_UINT(section, NAME_BACKEND_LATENCY, gem5.maxAccessesPerRow);
  STORE_NAME_UINT(section, NAME_ROW_BUFFER_SIZE, gem5.rowBufferSize);
  STORE_NAME_UINT(section, NAME_BANK_GROUP, gem5.bankGroup);
  STORE_NAME_BOOLEAN(section, NAME_ENABLE_POWERDOWN, gem5.enablePowerdown);
  STORE_NAME_BOOLEAN(section, NAME_USE_DLL, gem5.useDLL);
}

void Config::loadFrom(pugi::xml_node &section) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    auto name = node.attribute("name").value();

    if (strcmp(name, "sram") == 0 && isSection(node)) {
      loadSRAM(node);
    }
    else if (strcmp(name, "dram") == 0) {
      for (auto node2 = node.first_child(); node2;
           node2 = node2.next_sibling()) {
        auto name2 = node2.attribute("name").value();

        if (strcmp(name2, "struct") == 0 && isSection(node2)) {
          loadDRAMStructure(node2);
        }
        else if (strcmp(name2, "timing") == 0 && isSection(node2)) {
          loadDRAMTiming(node2);
        }
        else if (strcmp(name2, "power") == 0 && isSection(node2)) {
          loadDRAMPower(node2);
        }
        else if (strcmp(name2, "gem5") == 0 && isSection(node2)) {
          loadDRAMTiming(node2);
        }

        LOAD_NAME_UINT_TYPE(node2, NAME_MODEL, Model, dramModel);
      }
    }
  }
}

void Config::storeTo(pugi::xml_node &section) {
  pugi::xml_node node, node2;

  STORE_SECTION(section, "sram", node);
  storeSRAM(node);

  STORE_SECTION(section, "dram", node);
  STORE_NAME_UINT(node, NAME_MODEL, dramModel);

  STORE_SECTION(node, "struct", node2);
  storeDRAMStructure(node2);

  STORE_SECTION(node, "timing", node2);
  storeDRAMTiming(node2);

  STORE_SECTION(node, "power", node2);
  storeDRAMPower(node2);

  STORE_SECTION(node, "gem5", node2);
  storeTimingDRAM(node2);
}

void Config::update() {
  // Validate SRAM
  panic_if(sram.lineSize == 0, "Invalid line size");
  panic_if(sram.latency == 0, "Invalid latency");

  if (sram.size > 0) {
    panic_if(sram.size % sram.lineSize != 0, "Size not aligned");
  }
}

uint64_t Config::readUint(uint32_t idx) {
  switch (idx) {
    case DRAMModel:
      return dramModel;
  }

  return 0;
}

bool Config::writeUint(uint32_t idx, uint64_t value) {
  bool ret = true;

  switch (idx) {
    case DRAMModel:
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

Config::TimingDRAMConfig *Config::getTimingDRAM() {
  return &gem5;
}

}  // namespace SimpleSSD::Memory
