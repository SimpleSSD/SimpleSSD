// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "sim/config_reader.hh"

#include <catch2/catch.hpp>
#include <filesystem>

TEST_CASE("ConfigReader") {
  using namespace SimpleSSD;

  const uint64_t utest = 55;
  const float ftest = 55.55f;
  const bool btest = true;
  const std::string stest("test");
  const char path[] = "test_config.xml";

  {
    // Make empty ConfigReader object which is filled with default values
    ConfigReader reader;

    /*
     * Change all values to:
     *  55 for <int>
     *  55.55 for <float>
     *  true for <bool>
     *  "test" for <string>
     */
    {
      // Section::Simulation
      reader.writeUint(Section::Simulation, Config::Key::Controller, utest);
      reader.writeString(Section::Simulation, Config::Key::DebugFile, stest);
      reader.writeString(Section::Simulation, Config::Key::ErrorFile, stest);
      reader.writeString(Section::Simulation, Config::Key::OutputDirectory,
                         stest);
      reader.writeString(Section::Simulation, Config::Key::OutputFile, stest);

      // Section::CPU
      reader.writeUint(Section::CPU, CPU::Config::Key::Clock, utest);
      reader.writeBoolean(Section::CPU, CPU::Config::Key::UseDedicatedCore,
                          btest);
      reader.writeUint(Section::CPU, CPU::Config::Key::HILCore, utest);
      reader.writeUint(Section::CPU, CPU::Config::Key::ICLCore, utest);
      reader.writeUint(Section::CPU, CPU::Config::Key::FTLCore, utest);

      // Section::Memory
      reader.writeUint(Section::Memory, Memory::Config::Key::DRAMModel, utest);
      reader.writeUint(Section::Memory, Memory::Config::Key::SystemBusSpeed,
                       utest);

      auto sram = reader.getSRAM();
      sram->size = utest;
      sram->dataRate = utest;
      sram->dataWidth = utest;
      sram->clockSpeed = utest;
      sram->readCycles = utest;
      sram->writeCycles = utest;
      sram->pIDD = ftest;
      sram->pISB1 = ftest;
      sram->pVCC = ftest;

      auto dram = reader.getDRAM();
      dram->channel = utest;
      dram->rank = utest;
      dram->bank = utest;
      dram->chip = utest;
      dram->width = utest;
      dram->burstChop = utest;
      dram->burstLength = utest;
      dram->chipSize = utest;
      dram->rowSize = utest;

      auto timing = reader.getDRAMTiming();
      timing->tCK = utest;
      timing->tRAS = utest;
      timing->tRRD = utest;
      timing->tRCD = utest;
      timing->tCCD = utest;
      timing->tRP = utest;
      timing->tRPab = utest;
      timing->tRL = utest;
      timing->tWL = utest;
      timing->tDQSCK = utest;
      timing->tWR = utest;
      timing->tWTR = utest;
      timing->tRTP = utest;
      timing->tRFC = utest;
      timing->tRFCab = utest;
      timing->tREFI = utest;
      timing->tSR = utest;
      timing->tXSV = utest;
      timing->tFAW = utest;

      auto power = reader.getDRAMPower();
      power->pIDD0[0] = ftest;
      power->pIDD0[1] = ftest;
      power->pIDD2P0[0] = ftest;
      power->pIDD2P0[1] = ftest;
      power->pIDD2P1[0] = ftest;
      power->pIDD2P1[1] = ftest;
      power->pIDD2N[0] = ftest;
      power->pIDD2N[1] = ftest;
      power->pIDD3P0[0] = ftest;
      power->pIDD3P0[1] = ftest;
      power->pIDD3P1[0] = ftest;
      power->pIDD3P1[1] = ftest;
      power->pIDD3N[0] = ftest;
      power->pIDD3N[1] = ftest;
      power->pIDD4R[0] = ftest;
      power->pIDD4R[1] = ftest;
      power->pIDD4W[0] = ftest;
      power->pIDD4W[1] = ftest;
      power->pIDD5[0] = ftest;
      power->pIDD5[1] = ftest;
      power->pIDD6[0] = ftest;
      power->pIDD6[1] = ftest;
      power->pVDD[0] = ftest;
      power->pVDD[1] = ftest;

      auto controller = reader.getDRAMController();
      controller->readQueueSize = utest;
      controller->writeQueueSize = utest;
      controller->writeMinThreshold = ftest;
      controller->writeMaxThreshold = ftest;
      controller->minWriteBurst = utest;
      controller->schedulePolicy = (Memory::Config::MemoryScheduling)utest;
      controller->addressPolicy = (Memory::Config::AddressMapping)utest;
      controller->pagePolicy = (Memory::Config::PagePolicy)utest;

      // Section::HostInterface
      reader.writeUint(Section::HostInterface, HIL::Config::Key::WorkInterval,
                       utest);
      reader.writeUint(Section::HostInterface,
                       HIL::Config::Key::RequestQueueSize, utest);
      reader.writeUint(Section::HostInterface, HIL::Config::Key::PCIeGeneration,
                       utest);
      reader.writeUint(Section::HostInterface, HIL::Config::Key::PCIeLane,
                       utest);
      reader.writeUint(Section::HostInterface, HIL::Config::Key::SATAGeneration,
                       utest);
      reader.writeUint(Section::HostInterface, HIL::Config::Key::MPHYMode,
                       utest);
      reader.writeUint(Section::HostInterface, HIL::Config::Key::MPHYLane,
                       utest);
      reader.writeUint(Section::HostInterface, HIL::Config::Key::NVMeMaxSQ,
                       utest);
      reader.writeUint(Section::HostInterface, HIL::Config::Key::NVMeMaxCQ,
                       utest);
      reader.writeUint(Section::HostInterface, HIL::Config::Key::NVMeWRRHigh,
                       utest);
      reader.writeUint(Section::HostInterface, HIL::Config::Key::NVMeWRRMedium,
                       utest);
      reader.writeUint(Section::HostInterface,
                       HIL::Config::Key::NVMeMaxNamespace, utest);
      reader.writeUint(Section::HostInterface,
                       HIL::Config::Key::NVMeDefaultNamespace, utest);
      reader.writeBoolean(Section::HostInterface,
                          HIL::Config::Key::NVMeAttachDefaultNamespaces, btest);
      auto &nslist = reader.getNamespaceList();
      nslist.resize(1);
      nslist.at(0).nsid = (uint32_t)utest;
      nslist.at(0).lbaSize = (uint16_t)utest;
      nslist.at(0).capacity = utest;

      // Section::InternalCache
      reader.writeUint(Section::InternalCache, ICL::Config::Key::CacheMode,
                       btest);
      reader.writeUint(Section::InternalCache, ICL::Config::Key::CacheSize,
                       utest);
      reader.writeBoolean(Section::InternalCache,
                          ICL::Config::Key::EnablePrefetch, btest);
      reader.writeUint(Section::InternalCache, ICL::Config::Key::PrefetchMode,
                       utest);
      reader.writeUint(Section::InternalCache, ICL::Config::Key::PrefetchCount,
                       utest);
      reader.writeUint(Section::InternalCache, ICL::Config::Key::PrefetchRatio,
                       utest);
      reader.writeUint(Section::InternalCache, ICL::Config::Key::EvictPolicy,
                       utest);
      reader.writeUint(Section::InternalCache,
                       ICL::Config::Key::EvictGranularity, utest);
      reader.writeFloat(Section::InternalCache,
                        ICL::Config::Key::EvictThreshold, ftest);
      reader.writeUint(Section::InternalCache, ICL::Config::Key::CacheWaySize,
                       utest);

      // Section::FlashTranslation
      reader.writeUint(Section::FlashTranslation, FTL::Config::Key::MappingMode,
                       utest);
      reader.writeUint(Section::FlashTranslation, FTL::Config::Key::FillingMode,
                       utest);
      reader.writeUint(Section::FlashTranslation, FTL::Config::Key::GCMode,
                       utest);
      reader.writeFloat(Section::FlashTranslation, FTL::Config::Key::FillRatio,
                        ftest);
      reader.writeFloat(Section::FlashTranslation,
                        FTL::Config::Key::InvalidFillRatio, ftest);
      reader.writeUint(Section::FlashTranslation,
                       FTL::Config::Key::VictimSelectionPolicy, utest);
      reader.writeUint(Section::FlashTranslation,
                       FTL::Config::Key::SamplingFactor, utest);
      reader.writeFloat(Section::FlashTranslation,
                        FTL::Config::Key::ForegroundGCThreshold, ftest);
      reader.writeFloat(Section::FlashTranslation,
                        FTL::Config::Key::BackgroundGCThreshold, ftest * 2.f);
      reader.writeUint(Section::FlashTranslation,
                       FTL::Config::Key::IdleTimeForBackgroundGC, utest);
      reader.writeFloat(Section::FlashTranslation,
                        FTL::Config::Key::OverProvisioningRatio, ftest);
      reader.writeUint(Section::FlashTranslation,
                       FTL::Config::Key::SuperpageAllocation,
                       FIL::PageAllocation::Way | FIL::PageAllocation::Die);
      reader.writeBoolean(Section::FlashTranslation,
                          FTL::Config::Key::MergeReadModifyWrite, btest);

      // Section::FlashInterface
      reader.writeUint(Section::FlashInterface, FIL::Config::Key::Channel,
                       utest);
      reader.writeUint(Section::FlashInterface, FIL::Config::Key::Way, utest);
      reader.writeUint(Section::FlashInterface, FIL::Config::Key::Model, utest);
      reader.writeUint(Section::FlashInterface, FIL::Config::Key::Scheduler,
                       utest);

      auto nand = reader.getNANDStructure();
      nand->type = (FIL::Config::NANDType)utest;
      nand->nop = utest;
      nand->pageAllocation[0] = FIL::PageAllocation::Die;
      nand->pageAllocation[1] = FIL::PageAllocation::Way;
      nand->pageAllocation[2] = FIL::PageAllocation::Plane;
      nand->pageAllocation[3] = FIL::PageAllocation::Channel;
      nand->die = utest;
      nand->plane = utest;
      nand->block = utest;
      nand->page = utest;
      nand->pageSize = utest;
      nand->spareSize = utest;
      nand->dmaSpeed = utest;
      nand->dmaWidth = utest;

      auto ntiming = reader.getNANDTiming();
      ntiming->tADL = utest;
      ntiming->tCS = utest;
      ntiming->tDH = utest;
      ntiming->tDS = utest;
      ntiming->tRC = utest;
      ntiming->tRR = utest;
      ntiming->tWB = utest;
      ntiming->tWC = utest;
      ntiming->tWP = utest;
      ntiming->tCBSY = utest;
      ntiming->tDBSY = utest;
      ntiming->tRCBSY = utest;
      ntiming->tBERS = utest;
      ntiming->tPROG[0] = utest;
      ntiming->tPROG[1] = utest;
      ntiming->tPROG[2] = utest;
      ntiming->tR[0] = utest;
      ntiming->tR[1] = utest;
      ntiming->tR[2] = utest;

      auto npower = reader.getNANDPower();
      npower->pVCC = utest;
      npower->current.pICC1 = utest;
      npower->current.pICC2 = utest;
      npower->current.pICC3 = utest;
      npower->current.pICC4R = utest;
      npower->current.pICC4W = utest;
      npower->current.pICC5 = utest;
      npower->current.pISB = utest;
    }

    // Store configuration file
    reader.save(path);
  }

  {
    ConfigReader reader;

    reader.load(path, true);

    // Check values
    {
      // Section::Simulation
      REQUIRE(reader.readUint(Section::Simulation, Config::Key::Controller) ==
              utest);
      REQUIRE(reader.readString(Section::Simulation, Config::Key::DebugFile) ==
              stest);
      REQUIRE(reader.readString(Section::Simulation, Config::Key::ErrorFile) ==
              stest);
      REQUIRE(reader.readString(Section::Simulation,
                                Config::Key::OutputDirectory) == stest);
      REQUIRE(reader.readString(Section::Simulation, Config::Key::OutputFile) ==
              stest);

      // Section::CPU
      REQUIRE(reader.readUint(Section::CPU, CPU::Config::Key::Clock) == utest);
      REQUIRE(reader.readBoolean(Section::CPU,
                                 CPU::Config::Key::UseDedicatedCore) == btest);
      REQUIRE(reader.readUint(Section::CPU, CPU::Config::Key::HILCore) ==
              utest);
      REQUIRE(reader.readUint(Section::CPU, CPU::Config::Key::ICLCore) ==
              utest);
      REQUIRE(reader.readUint(Section::CPU, CPU::Config::Key::FTLCore) ==
              utest);

      // Section::Memory
      REQUIRE(reader.readUint(Section::Memory,
                              Memory::Config::Key::DRAMModel) == utest);
      REQUIRE(reader.readUint(Section::Memory,
                              Memory::Config::Key::SystemBusSpeed) == utest);

      auto sram = reader.getSRAM();
      REQUIRE(sram->size == utest);
      REQUIRE(sram->dataRate == utest);
      REQUIRE(sram->dataWidth == utest);
      REQUIRE(sram->clockSpeed == utest);
      REQUIRE(sram->readCycles == utest);
      REQUIRE(sram->writeCycles == utest);
      REQUIRE(sram->pIDD == ftest);
      REQUIRE(sram->pISB1 == ftest);
      REQUIRE(sram->pVCC == ftest);

      auto dram = reader.getDRAM();
      REQUIRE(dram->channel == utest);
      REQUIRE(dram->rank == utest);
      REQUIRE(dram->bank == utest);
      REQUIRE(dram->chip == utest);
      REQUIRE(dram->width == utest);
      REQUIRE(dram->burstChop == utest);
      REQUIRE(dram->burstLength == utest);
      REQUIRE(dram->chipSize == utest);
      REQUIRE(dram->rowSize == utest);

      auto timing = reader.getDRAMTiming();
      REQUIRE(timing->tCK == utest);
      REQUIRE(timing->tRAS == utest);
      REQUIRE(timing->tRRD == utest);
      REQUIRE(timing->tRCD == utest);
      REQUIRE(timing->tCCD == utest);
      REQUIRE(timing->tRP == utest);
      REQUIRE(timing->tRPab == utest);
      REQUIRE(timing->tRL == utest);
      REQUIRE(timing->tWL == utest);
      REQUIRE(timing->tDQSCK == utest);
      REQUIRE(timing->tWR == utest);
      REQUIRE(timing->tWTR == utest);
      REQUIRE(timing->tRTP == utest);
      REQUIRE(timing->tRFC == utest);
      REQUIRE(timing->tRFCab == utest);
      REQUIRE(timing->tREFI == utest);
      REQUIRE(timing->tSR == utest);
      REQUIRE(timing->tXSV == utest);
      REQUIRE(timing->tFAW == utest);

      auto power = reader.getDRAMPower();
      REQUIRE(power->pIDD0[0] == ftest);
      REQUIRE(power->pIDD0[1] == ftest);
      REQUIRE(power->pIDD2P0[0] == ftest);
      REQUIRE(power->pIDD2P0[1] == ftest);
      REQUIRE(power->pIDD2P1[0] == ftest);
      REQUIRE(power->pIDD2P1[1] == ftest);
      REQUIRE(power->pIDD2N[0] == ftest);
      REQUIRE(power->pIDD2N[1] == ftest);
      REQUIRE(power->pIDD3P0[0] == ftest);
      REQUIRE(power->pIDD3P0[1] == ftest);
      REQUIRE(power->pIDD3P1[0] == ftest);
      REQUIRE(power->pIDD3P1[1] == ftest);
      REQUIRE(power->pIDD3N[0] == ftest);
      REQUIRE(power->pIDD3N[1] == ftest);
      REQUIRE(power->pIDD4R[0] == ftest);
      REQUIRE(power->pIDD4R[1] == ftest);
      REQUIRE(power->pIDD4W[0] == ftest);
      REQUIRE(power->pIDD4W[1] == ftest);
      REQUIRE(power->pIDD5[0] == ftest);
      REQUIRE(power->pIDD5[1] == ftest);
      REQUIRE(power->pIDD6[0] == ftest);
      REQUIRE(power->pIDD6[1] == ftest);
      REQUIRE(power->pVDD[0] == ftest);
      REQUIRE(power->pVDD[1] == ftest);

      auto controller = reader.getDRAMController();
      REQUIRE(controller->readQueueSize == utest);
      REQUIRE(controller->writeQueueSize == utest);
      REQUIRE(controller->writeMinThreshold == ftest);
      REQUIRE(controller->writeMaxThreshold == ftest);
      REQUIRE(controller->minWriteBurst == utest);
      REQUIRE(controller->schedulePolicy ==
              (Memory::Config::MemoryScheduling)utest);
      REQUIRE(controller->addressPolicy ==
              (Memory::Config::AddressMapping)utest);
      REQUIRE(controller->pagePolicy == (Memory::Config::PagePolicy)utest);

      // Section::HostInterface
      REQUIRE(reader.readUint(Section::HostInterface,
                              HIL::Config::Key::WorkInterval) == utest);
      REQUIRE(reader.readUint(Section::HostInterface,
                              HIL::Config::Key::RequestQueueSize) == utest);
      // utest + 1 because it handled in update()
      REQUIRE(reader.readUint(Section::HostInterface,
                              HIL::Config::Key::PCIeGeneration) == utest + 1);
      REQUIRE(reader.readUint(Section::HostInterface,
                              HIL::Config::Key::PCIeLane) == utest);
      // utest + 1 because it handled in update()
      REQUIRE(reader.readUint(Section::HostInterface,
                              HIL::Config::Key::SATAGeneration) == utest + 1);
      REQUIRE(reader.readUint(Section::HostInterface,
                              HIL::Config::Key::MPHYMode) == utest);
      REQUIRE(reader.readUint(Section::HostInterface,
                              HIL::Config::Key::MPHYLane) == utest);
      REQUIRE(reader.readUint(Section::HostInterface,
                              HIL::Config::Key::NVMeMaxSQ) == utest);
      REQUIRE(reader.readUint(Section::HostInterface,
                              HIL::Config::Key::NVMeMaxCQ) == utest);
      REQUIRE(reader.readUint(Section::HostInterface,
                              HIL::Config::Key::NVMeWRRHigh) == utest);
      REQUIRE(reader.readUint(Section::HostInterface,
                              HIL::Config::Key::NVMeWRRMedium) == utest);
      REQUIRE(reader.readUint(Section::HostInterface,
                              HIL::Config::Key::NVMeMaxNamespace) == utest);
      REQUIRE(reader.readUint(Section::HostInterface,
                              HIL::Config::Key::NVMeDefaultNamespace) == utest);
      REQUIRE(reader.readBoolean(
                  Section::HostInterface,
                  HIL::Config::Key::NVMeAttachDefaultNamespaces) == btest);

      auto &nslist = reader.getNamespaceList();

      REQUIRE(nslist.size() == 1);

      REQUIRE(nslist.at(0).nsid == (uint32_t)utest);
      REQUIRE(nslist.at(0).lbaSize == (uint16_t)utest);
      REQUIRE(nslist.at(0).capacity == utest);

      // Section::InternalCache
      REQUIRE(reader.readUint(Section::InternalCache,
                              ICL::Config::Key::CacheMode) == btest);
      REQUIRE(reader.readUint(Section::InternalCache,
                              ICL::Config::Key::CacheSize) == utest);
      REQUIRE(reader.readBoolean(Section::InternalCache,
                                 ICL::Config::Key::EnablePrefetch) == btest);
      REQUIRE(reader.readUint(Section::InternalCache,
                              ICL::Config::Key::PrefetchMode) == utest);
      REQUIRE(reader.readUint(Section::InternalCache,
                              ICL::Config::Key::PrefetchCount) == utest);
      REQUIRE(reader.readUint(Section::InternalCache,
                              ICL::Config::Key::PrefetchRatio) == utest);
      REQUIRE(reader.readUint(Section::InternalCache,
                              ICL::Config::Key::EvictPolicy) == utest);
      REQUIRE(reader.readUint(Section::InternalCache,
                              ICL::Config::Key::EvictGranularity) == utest);
      REQUIRE(reader.readFloat(Section::InternalCache,
                               ICL::Config::Key::EvictThreshold) == ftest);
      REQUIRE(reader.readUint(Section::InternalCache,
                              ICL::Config::Key::CacheWaySize) == utest);

      // Section::FlashTranslation
      REQUIRE(reader.readUint(Section::FlashTranslation,
                              FTL::Config::Key::MappingMode) == utest);
      REQUIRE(reader.readUint(Section::FlashTranslation,
                              FTL::Config::Key::FillingMode) == utest);
      REQUIRE(reader.readUint(Section::FlashTranslation,
                              FTL::Config::Key::GCMode) == utest);
      REQUIRE(reader.readFloat(Section::FlashTranslation,
                               FTL::Config::Key::FillRatio) == ftest);
      REQUIRE(reader.readFloat(Section::FlashTranslation,
                               FTL::Config::Key::InvalidFillRatio) == ftest);
      REQUIRE(reader.readUint(Section::FlashTranslation,
                              FTL::Config::Key::VictimSelectionPolicy) ==
              utest);
      REQUIRE(reader.readUint(Section::FlashTranslation,
                              FTL::Config::Key::SamplingFactor) == utest);
      REQUIRE(reader.readFloat(Section::FlashTranslation,
                               FTL::Config::Key::ForegroundGCThreshold) ==
              ftest);
      REQUIRE(reader.readFloat(Section::FlashTranslation,
                               FTL::Config::Key::BackgroundGCThreshold) ==
              ftest * 2.f);
      REQUIRE(reader.readUint(Section::FlashTranslation,
                              FTL::Config::Key::IdleTimeForBackgroundGC) ==
              utest);
      REQUIRE(reader.readFloat(Section::FlashTranslation,
                               FTL::Config::Key::OverProvisioningRatio) ==
              ftest);
      // Not tested because it handled in update()
      // REQUIRE(reader.readUint(Section::FlashTranslation,
      //                         FTL::Config::Key::SuperpageAllocation) ==
      //         (FIL::PageAllocation::Way | FIL::PageAllocation::Die));
      REQUIRE(reader.readBoolean(Section::FlashTranslation,
                                 FTL::Config::Key::MergeReadModifyWrite) ==
              btest);

      // Section::FlashInterface
      REQUIRE(reader.readUint(Section::FlashInterface,
                              FIL::Config::Key::Channel) == utest);
      REQUIRE(reader.readUint(Section::FlashInterface, FIL::Config::Key::Way) ==
              utest);
      REQUIRE(reader.readUint(Section::FlashInterface,
                              FIL::Config::Key::Model) == utest);
      REQUIRE(reader.readUint(Section::FlashInterface,
                              FIL::Config::Key::Scheduler) == utest);

      auto nand = reader.getNANDStructure();
      REQUIRE(nand->type == (FIL::Config::NANDType)utest);
      REQUIRE(nand->nop == utest);
      // Not tested because it handled in update()
      // REQUIRE(nand->pageAllocation[0] == FIL::PageAllocation::Die);
      // REQUIRE(nand->pageAllocation[1] == FIL::PageAllocation::Way);
      // REQUIRE(nand->pageAllocation[2] == FIL::PageAllocation::Plane);
      // REQUIRE(nand->pageAllocation[3] == FIL::PageAllocation::Channel);
      REQUIRE(nand->die == utest);
      REQUIRE(nand->plane == utest);
      REQUIRE(nand->block == utest);
      REQUIRE(nand->page == utest);
      REQUIRE(nand->pageSize == utest);
      REQUIRE(nand->spareSize == utest);
      REQUIRE(nand->dmaSpeed == utest);
      REQUIRE(nand->dmaWidth == utest);

      auto ntiming = reader.getNANDTiming();
      REQUIRE(ntiming->tADL == utest);
      REQUIRE(ntiming->tCS == utest);
      REQUIRE(ntiming->tDH == utest);
      REQUIRE(ntiming->tDS == utest);
      REQUIRE(ntiming->tRC == utest);
      REQUIRE(ntiming->tRR == utest);
      REQUIRE(ntiming->tWB == utest);
      REQUIRE(ntiming->tWC == utest);
      REQUIRE(ntiming->tWP == utest);
      REQUIRE(ntiming->tCBSY == utest);
      REQUIRE(ntiming->tDBSY == utest);
      REQUIRE(ntiming->tRCBSY == utest);
      REQUIRE(ntiming->tBERS == utest);
      REQUIRE(ntiming->tPROG[0] == utest);
      REQUIRE(ntiming->tPROG[1] == utest);
      REQUIRE(ntiming->tPROG[2] == utest);
      REQUIRE(ntiming->tR[0] == utest);
      REQUIRE(ntiming->tR[1] == utest);
      REQUIRE(ntiming->tR[2] == utest);

      auto npower = reader.getNANDPower();
      REQUIRE(npower->pVCC == utest);
      REQUIRE(npower->current.pICC1 == utest);
      REQUIRE(npower->current.pICC2 == utest);
      REQUIRE(npower->current.pICC3 == utest);
      REQUIRE(npower->current.pICC4R == utest);
      REQUIRE(npower->current.pICC4W == utest);
      REQUIRE(npower->current.pICC5 == utest);
      REQUIRE(npower->current.pISB == utest);
    }
  }

  // Remove temporal file
  try {
    std::filesystem::remove(path);
  }
  catch (const std::exception &) {
    // Do nothing
  }
}
