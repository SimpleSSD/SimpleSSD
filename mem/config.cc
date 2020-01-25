// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "mem/config.hh"

#include "util/algorithm.hh"

namespace SimpleSSD::Memory {

const char NAME_MODEL[] = "Model";
const char NAME_BUS_CLOCK[] = "BusClock";
const char NAME_SIZE[] = "Size";
const char NAME_WAY_SIZE[] = "WaySize";
const char NAME_TAG_LATENCY[] = "TagLatency";
const char NAME_DATA_LATENCY[] = "DataLatency";
const char NAME_RESPONSE_LATENCY[] = "ResponseLatency";
const char NAME_CLOCK[] = "Clock";
const char NAME_DATA_RATE[] = "DataRate";
const char NAME_DATA_WIDTH[] = "DataWidth";
const char NAME_READ_LATENCY[] = "ReadLatency";
const char NAME_WRITE_LATENCY[] = "WriteLatency";
const char NAME_IDD[] = "IDD";
const char NAME_ISB[] = "ISB1";
const char NAME_VCC[] = "VCC";
const char NAME_CHANNEL[] = "Channel";
const char NAME_RANK[] = "Rank";
const char NAME_BANK[] = "Bank";
const char NAME_CHIP[] = "Chip";
const char NAME_BUS_WIDTH[] = "BusWidth";
const char NAME_BURST_CHOP[] = "BurstChop";
const char NAME_BURST_LENGTH[] = "BurstLength";
const char NAME_CHIP_SIZE[] = "ChipSize";
const char NAME_ROWBUFFER_SIZE[] = "RowBufferSize";

const char NAME_TCK[] = "tCK";
const char NAME_TRAS[] = "tRAS";
const char NAME_TRRD[] = "tRRD";
const char NAME_TRCD[] = "tRCD";
const char NAME_TCCD[] = "tCCD";
const char NAME_TRP[] = "tRP";
const char NAME_TRPab[] = "tRPab";
const char NAME_TRL[] = "tRL";
const char NAME_TWL[] = "tWL";
const char NAME_TDQSCK[] = "tDQSCK";
const char NAME_TWR[] = "tWR";
const char NAME_TWTR[] = "tWTR";
const char NAME_TRTP[] = "tRTP";
const char NAME_TRFC[] = "tRFC";
const char NAME_TRFCab[] = "tRFCab";
const char NAME_TREFI[] = "tREFI";
const char NAME_TSR[] = "tSR";
const char NAME_TXSV[] = "tXSV";
const char NAME_TFAW[] = "tFAW";

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

const char NAME_WRITE_QUEUE_SIZE[] = "WriteQueueSize";
const char NAME_READ_QUEUE_SIZE[] = "ReadQueueSize";
const char NAME_SCHEDULING[] = "Scheduling";
const char NAME_MAPPING[] = "Mapping";
const char NAME_PAGE_POLICY[] = "PagePolicy";
const char NAME_WRITE_MAX_THRESHOLD[] = "ForceWriteThreshold";
const char NAME_WRITE_MIN_THRESHOLD[] = "WriteThreshold";
const char NAME_MIN_WRITE_BURST[] = "MinWriteBurst";

Config::Config() {
  /* Default memory subsystem */
  systemBusSpeed = 200000000;

  /* gem5's L2 cache config */
  llc.size = 262144;
  llc.way = 8;
  llc.tagCycles = 20;
  llc.dataCycles = 20;
  llc.responseCycles = 20;

  /* CY7C1550KV18 DDR SRAM @ max. 450MHz 2M x36 */
  sram.size = 75497472;
  sram.clockSpeed = 400000000;
  sram.dataRate = 2;
  sram.dataWidth = 36;
  sram.readCycles = 2;
  sram.writeCycles = 2;
  sram.pIDD = 750.f;
  sram.pISB1 = 320.f;
  sram.pVCC = 1.8f;

  /* MT53B512M32 LPDDR4-3200 512Mb x32 */
  dramModel = Model::LPDDR4;

  dram.channel = 2;
  dram.rank = 2;
  dram.bank = 8;
  dram.chip = 1;
  dram.width = 16;
  dram.burstChop = 16;
  dram.burstLength = 32;
  dram.chipSize = 1073741824;
  dram.rowSize = 2048;

  timing.tCK = 625;
  timing.tRAS = MAX(32000, 3 * timing.tCK);
  timing.tRCD = MAX(18000, 4 * timing.tCK);
  timing.tRP = MAX(18000, 3 * timing.tCK);
  timing.tRPab = MAX(21000, 3 * timing.tCK);
  timing.tRRD = MAX(10000, 4 * timing.tCK);
  timing.tCCD = 8 * timing.tCK;
  timing.tRL = 28 * timing.tCK;
  timing.tWL = 14 * timing.tCK;
  timing.tDQSCK = 3500;
  timing.tWR = MAX(18000, 4 * timing.tCK);
  timing.tWTR = MAX(10000, 8 * timing.tCK);
  timing.tRTP = MAX(7500, 8 * timing.tCK);
  timing.tRFC = 14000;
  timing.tRFCab = 28000;
  timing.tREFI = 3904000;
  timing.tSR = MAX(15000, 3 * timing.tCK);
  timing.tXSV = MAX(timing.tRFCab + 7500, 2 * timing.tCK);
  timing.tFAW = 40000;

  power.pIDD0[0] = 7.f;
  power.pIDD0[1] = 80.f;
  power.pIDD2P0[0] = 2.f;
  power.pIDD2P0[1] = 3.5f;
  power.pIDD2P1[0] = 2.f;
  power.pIDD2P1[1] = 3.5f;
  power.pIDD2N[0] = 2.f;
  power.pIDD2N[1] = 45.f;
  power.pIDD3P0[0] = 2.f;
  power.pIDD3P0[1] = 10.f;
  power.pIDD3P1[0] = 2.f;
  power.pIDD3P1[1] = 10.f;
  power.pIDD3N[0] = 4.f;
  power.pIDD3N[1] = 57.f;
  power.pIDD4R[0] = 5.f;
  power.pIDD4R[1] = 450.f;
  power.pIDD4W[0] = 5.f;
  power.pIDD4W[1] = 350.f;
  power.pIDD5[0] = 20.f;
  power.pIDD5[1] = 170.f;
  power.pIDD6[0] = 0.4f;
  power.pIDD6[1] = 1.7f;
  power.pVDD[0] = 1.8f;
  power.pVDD[1] = 1.1f;

  controller.readQueueSize = 64;
  controller.writeQueueSize = 64;
  controller.writeMinThreshold = 0.5;
  controller.writeMaxThreshold = 0.85;
  controller.minWriteBurst = 16;
  controller.schedulePolicy = MemoryScheduling::FRFCFS;
  controller.addressPolicy = AddressMapping::RoRaBaCoCh;
  controller.pagePolicy = PagePolicy::OpenAdaptive;
}

void Config::loadCache(pugi::xml_node &section, CacheConfig &cache) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_UINT_TYPE(node, NAME_SIZE, uint32_t, cache.size);
    LOAD_NAME_UINT_TYPE(node, NAME_WAY_SIZE, uint16_t, cache.way);
    LOAD_NAME_UINT_TYPE(node, NAME_TAG_LATENCY, uint16_t, cache.tagCycles);
    LOAD_NAME_UINT_TYPE(node, NAME_DATA_LATENCY, uint16_t, cache.dataCycles);
    LOAD_NAME_UINT_TYPE(node, NAME_RESPONSE_LATENCY, uint16_t,
                        cache.responseCycles);
  }
}

void Config::loadSRAM(pugi::xml_node &section) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_UINT_TYPE(node, NAME_SIZE, uint32_t, sram.size);
    LOAD_NAME_UINT_TYPE(node, NAME_DATA_RATE, uint16_t, sram.dataRate);
    LOAD_NAME_UINT_TYPE(node, NAME_DATA_WIDTH, uint16_t, sram.dataWidth);
    LOAD_NAME_UINT(node, NAME_CLOCK, sram.clockSpeed);
    LOAD_NAME_UINT_TYPE(node, NAME_READ_LATENCY, uint16_t, sram.readCycles);
    LOAD_NAME_UINT_TYPE(node, NAME_WRITE_LATENCY, uint16_t, sram.writeCycles);
    LOAD_NAME_FLOAT(node, NAME_IDD, sram.pIDD);
    LOAD_NAME_FLOAT(node, NAME_ISB, sram.pISB1);
    LOAD_NAME_FLOAT(node, NAME_VCC, sram.pVCC);
  }
}

void Config::loadDRAMStructure(pugi::xml_node &section) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_UINT_TYPE(node, NAME_CHANNEL, uint8_t, dram.channel);
    LOAD_NAME_UINT_TYPE(node, NAME_RANK, uint8_t, dram.rank);
    LOAD_NAME_UINT_TYPE(node, NAME_BANK, uint8_t, dram.bank);
    LOAD_NAME_UINT_TYPE(node, NAME_CHIP, uint8_t, dram.chip);
    LOAD_NAME_UINT_TYPE(node, NAME_BUS_WIDTH, uint16_t, dram.width);
    LOAD_NAME_UINT_TYPE(node, NAME_BURST_CHOP, uint16_t, dram.burstChop);
    LOAD_NAME_UINT_TYPE(node, NAME_BURST_LENGTH, uint16_t, dram.burstLength);
    LOAD_NAME_UINT(node, NAME_CHIP_SIZE, dram.chipSize);
    LOAD_NAME_UINT(node, NAME_ROWBUFFER_SIZE, dram.rowSize);
  }
}

void Config::loadDRAMTiming(pugi::xml_node &section) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    LOAD_NAME_TIME_TYPE(node, NAME_TCK, uint32_t, timing.tCK);
    LOAD_NAME_TIME_TYPE(node, NAME_TRAS, uint32_t, timing.tRAS);
    LOAD_NAME_TIME_TYPE(node, NAME_TRRD, uint32_t, timing.tRRD);
    LOAD_NAME_TIME_TYPE(node, NAME_TRCD, uint32_t, timing.tRCD);
    LOAD_NAME_TIME_TYPE(node, NAME_TCCD, uint32_t, timing.tCCD);
    LOAD_NAME_TIME_TYPE(node, NAME_TRP, uint32_t, timing.tRP);
    LOAD_NAME_TIME_TYPE(node, NAME_TRPab, uint32_t, timing.tRPab);
    LOAD_NAME_TIME_TYPE(node, NAME_TRL, uint32_t, timing.tRL);
    LOAD_NAME_TIME_TYPE(node, NAME_TWL, uint32_t, timing.tWL);
    LOAD_NAME_TIME_TYPE(node, NAME_TDQSCK, uint32_t, timing.tDQSCK);
    LOAD_NAME_TIME_TYPE(node, NAME_TWR, uint32_t, timing.tWR);
    LOAD_NAME_TIME_TYPE(node, NAME_TWTR, uint32_t, timing.tWTR);
    LOAD_NAME_TIME_TYPE(node, NAME_TRTP, uint32_t, timing.tRTP);
    LOAD_NAME_TIME_TYPE(node, NAME_TRFC, uint32_t, timing.tRFC);
    LOAD_NAME_TIME_TYPE(node, NAME_TRFCab, uint32_t, timing.tRFCab);
    LOAD_NAME_TIME_TYPE(node, NAME_TREFI, uint32_t, timing.tREFI);
    LOAD_NAME_TIME_TYPE(node, NAME_TSR, uint32_t, timing.tSR);
    LOAD_NAME_TIME_TYPE(node, NAME_TXSV, uint32_t, timing.tXSV);
    LOAD_NAME_TIME_TYPE(node, NAME_TFAW, uint32_t, timing.tFAW);
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
    LOAD_NAME_UINT_TYPE(node, NAME_WRITE_QUEUE_SIZE, uint32_t,
                        controller.writeQueueSize);
    LOAD_NAME_UINT_TYPE(node, NAME_READ_QUEUE_SIZE, uint32_t,
                        controller.readQueueSize);
    LOAD_NAME_FLOAT(node, NAME_WRITE_MIN_THRESHOLD,
                    controller.writeMinThreshold);
    LOAD_NAME_FLOAT(node, NAME_WRITE_MAX_THRESHOLD,
                    controller.writeMaxThreshold);
    LOAD_NAME_UINT_TYPE(node, NAME_MIN_WRITE_BURST, uint32_t,
                        controller.minWriteBurst);
    LOAD_NAME_UINT_TYPE(node, NAME_SCHEDULING, MemoryScheduling,
                        controller.schedulePolicy);
    LOAD_NAME_UINT_TYPE(node, NAME_MAPPING, AddressMapping,
                        controller.addressPolicy);
    LOAD_NAME_UINT_TYPE(node, NAME_PAGE_POLICY, PagePolicy,
                        controller.pagePolicy);
  }
}

void Config::storeCache(pugi::xml_node &section, CacheConfig &cache) {
  STORE_NAME_UINT(section, NAME_SIZE, cache.size);
  STORE_NAME_UINT(section, NAME_WAY_SIZE, cache.way);
  STORE_NAME_UINT(section, NAME_TAG_LATENCY, cache.tagCycles);
  STORE_NAME_UINT(section, NAME_DATA_LATENCY, cache.dataCycles);
  STORE_NAME_UINT(section, NAME_RESPONSE_LATENCY, cache.responseCycles);
}

void Config::storeSRAM(pugi::xml_node &section) {
  STORE_NAME_UINT(section, NAME_SIZE, sram.size);
  STORE_NAME_UINT(section, NAME_DATA_RATE, sram.dataRate);
  STORE_NAME_UINT(section, NAME_DATA_WIDTH, sram.dataWidth);
  STORE_NAME_UINT(section, NAME_CLOCK, sram.clockSpeed);
  STORE_NAME_UINT(section, NAME_READ_LATENCY, sram.readCycles);
  STORE_NAME_UINT(section, NAME_WRITE_LATENCY, sram.writeCycles);
  STORE_NAME_FLOAT(section, NAME_IDD, sram.pIDD);
  STORE_NAME_FLOAT(section, NAME_ISB, sram.pISB1);
  STORE_NAME_FLOAT(section, NAME_VCC, sram.pVCC);
}

void Config::storeDRAMStructure(pugi::xml_node &section) {
  STORE_NAME_UINT(section, NAME_CHANNEL, dram.channel);
  STORE_NAME_UINT(section, NAME_RANK, dram.rank);
  STORE_NAME_UINT(section, NAME_BANK, dram.bank);
  STORE_NAME_UINT(section, NAME_CHIP, dram.chip);
  STORE_NAME_UINT(section, NAME_BUS_WIDTH, dram.width);
  STORE_NAME_UINT(section, NAME_BURST_CHOP, dram.burstChop);
  STORE_NAME_UINT(section, NAME_BURST_LENGTH, dram.burstLength);
  STORE_NAME_UINT(section, NAME_CHIP_SIZE, dram.chipSize);
  STORE_NAME_UINT(section, NAME_ROWBUFFER_SIZE, dram.rowSize);
}

void Config::storeDRAMTiming(pugi::xml_node &section) {
  STORE_NAME_TIME(section, NAME_TCK, timing.tCK);
  STORE_NAME_TIME(section, NAME_TRAS, timing.tRAS);
  STORE_NAME_TIME(section, NAME_TRRD, timing.tRRD);
  STORE_NAME_TIME(section, NAME_TRCD, timing.tRCD);
  STORE_NAME_TIME(section, NAME_TCCD, timing.tCCD);
  STORE_NAME_TIME(section, NAME_TRP, timing.tRP);
  STORE_NAME_TIME(section, NAME_TRPab, timing.tRPab);
  STORE_NAME_TIME(section, NAME_TRL, timing.tRL);
  STORE_NAME_TIME(section, NAME_TWL, timing.tWL);
  STORE_NAME_TIME(section, NAME_TDQSCK, timing.tDQSCK);
  STORE_NAME_TIME(section, NAME_TWR, timing.tWR);
  STORE_NAME_TIME(section, NAME_TWTR, timing.tWTR);
  STORE_NAME_TIME(section, NAME_TRTP, timing.tRTP);
  STORE_NAME_TIME(section, NAME_TRFC, timing.tRFC);
  STORE_NAME_TIME(section, NAME_TRFCab, timing.tRFCab);
  STORE_NAME_TIME(section, NAME_TREFI, timing.tREFI);
  STORE_NAME_TIME(section, NAME_TSR, timing.tSR);
  STORE_NAME_TIME(section, NAME_TXSV, timing.tXSV);
  STORE_NAME_TIME(section, NAME_TFAW, timing.tFAW);
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
  STORE_NAME_UINT(section, NAME_WRITE_QUEUE_SIZE, controller.writeQueueSize);
  STORE_NAME_UINT(section, NAME_READ_QUEUE_SIZE, controller.readQueueSize);
  STORE_NAME_FLOAT(section, NAME_WRITE_MIN_THRESHOLD,
                   controller.writeMinThreshold);
  STORE_NAME_FLOAT(section, NAME_WRITE_MAX_THRESHOLD,
                   controller.writeMaxThreshold);
  STORE_NAME_UINT(section, NAME_MIN_WRITE_BURST, controller.minWriteBurst);
  STORE_NAME_UINT(section, NAME_SCHEDULING, controller.schedulePolicy);
  STORE_NAME_UINT(section, NAME_MAPPING, controller.addressPolicy);
  STORE_NAME_UINT(section, NAME_PAGE_POLICY, controller.pagePolicy);
}

void Config::loadFrom(pugi::xml_node &section) {
  for (auto node = section.first_child(); node; node = node.next_sibling()) {
    auto name = node.attribute("name").value();

    LOAD_NAME_UINT(node, NAME_BUS_CLOCK, systemBusSpeed);

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
        else if (strcmp(name2, "controller") == 0 && isSection(node2)) {
          loadTimingDRAM(node2);
        }

        LOAD_NAME_UINT_TYPE(node2, NAME_MODEL, Model, dramModel);
      }
    }
  }
}

void Config::storeTo(pugi::xml_node &section) {
  pugi::xml_node node, node2;

  STORE_NAME_UINT(section, NAME_BUS_CLOCK, systemBusSpeed);

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

  STORE_SECTION(node, "controller", node2);
  storeTimingDRAM(node2);
}

void Config::update() {
  if (dramModel == Model::LPDDR4) {
    panic_if(dram.channel % 2 != 0, "LPDDR4 has 2n channels.");

    // Fix for controller
    dram.channel /= 2;
    dram.width *= 2;
  }
}

uint64_t Config::readUint(uint32_t idx) {
  switch (idx) {
    case DRAMModel:
      return dramModel;
    case SystemBusSpeed:
      return systemBusSpeed;
  }

  return 0;
}

bool Config::writeUint(uint32_t idx, uint64_t value) {
  bool ret = true;

  switch (idx) {
    case DRAMModel:
      dramModel = (Model)value;
      break;
    case SystemBusSpeed:
      systemBusSpeed = value;
      break;
    default:
      ret = false;
      break;
  }

  return ret;
}

Config::CacheConfig *Config::getLLC() {
  return &llc;
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

Config::DRAMController *Config::getDRAMController() {
  return &controller;
}

}  // namespace SimpleSSD::Memory
