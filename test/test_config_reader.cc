#include <iostream>
#include <string>

#include "sim/config_reader.hh"

using namespace SimpleSSD;

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << " Usage: " << argv[0] << " <output xml file> <input xml file>"
              << std::endl;
  }

  {
    ConfigReader reader;

    // Change all values to <int: 55> <float: 55.55> <bool: true> <string: test>
    {
      const uint64_t utest = 55;
      const float ftest = 55.55f;
      const bool btest = true;
      const std::string stest("test");

      // Section::Simulation
      reader.writeString(Section::Simulation, Config::Key::OutputDirectory,
                         stest);
      reader.writeString(Section::Simulation, Config::Key::OutputFile, stest);
      reader.writeString(Section::Simulation, Config::Key::ErrorFile, stest);
      reader.writeString(Section::Simulation, Config::Key::DebugFile, stest);
      reader.writeUint(Section::Simulation, Config::Key::Controller, utest);

      // Section::CPU
      reader.writeUint(Section::CPU, CPU::Config::Key::Clock, utest);
      reader.writeBoolean(Section::CPU, CPU::Config::Key::UseDedicatedCore,
                          btest);
      reader.writeUint(Section::CPU, CPU::Config::Key::HILCore, utest);
      reader.writeUint(Section::CPU, CPU::Config::Key::ICLCore, utest);
      reader.writeUint(Section::CPU, CPU::Config::Key::FTLCore, utest);

      // Section::Memory
      reader.writeUint(Section::Memory, Memory::Config::Key::DRAMModel, utest);

      auto sram = reader.getSRAM();
      sram->size = utest;
      sram->lineSize = utest;
      sram->latency = utest;

      auto dram = reader.getDRAM();
      dram->channel = (uint8_t)utest;
      dram->rank = (uint8_t)utest;
      dram->bank = (uint8_t)utest;
      dram->chip = (uint8_t)utest;
      dram->width = (uint16_t)utest;
      dram->burst = (uint16_t)utest;
      dram->chipSize = (uint64_t)utest;
      dram->pageSize = (uint32_t)utest;
      dram->useDLL = btest;
      dram->activationLimit = (uint32_t)utest;

      auto timing = reader.getDRAMTiming();
      timing->tCK = (uint32_t)utest;
      timing->tRCD = (uint32_t)utest;
      timing->tCL = (uint32_t)utest;
      timing->tRP = (uint32_t)utest;
      timing->tRAS = (uint32_t)utest;
      timing->tWR = (uint32_t)utest;
      timing->tRTP = (uint32_t)utest;
      timing->tBURST = (uint32_t)utest;
      timing->tCCD_L = (uint32_t)utest;
      timing->tCCD_L_WR = (uint32_t)utest;
      timing->tRFC = (uint32_t)utest;
      timing->tREFI = (uint32_t)utest;
      timing->tWTR = (uint32_t)utest;
      timing->tRTW = (uint32_t)utest;
      timing->tCS = (uint32_t)utest;
      timing->tRRD = (uint32_t)utest;
      timing->tRRD_L = (uint32_t)utest;
      timing->tXAW = (uint32_t)utest;
      timing->tXP = (uint32_t)utest;
      timing->tXPDLL = (uint32_t)utest;
      timing->tXS = (uint32_t)utest;
      timing->tXSDLL = (uint32_t)utest;

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

      auto gem5 = reader.getTimingDRAM();
      gem5->writeBufferSize = (uint32_t)utest;
      gem5->readBufferSize = (uint32_t)utest;
      gem5->forceWriteThreshold = ftest;
      gem5->startWriteThreshold = ftest;
      gem5->minWriteBurst = (uint32_t)utest;
      gem5->scheduling = (Memory::Config::MemoryScheduling)utest;
      gem5->mapping = (Memory::Config::AddressMapping)utest;
      gem5->policy = (Memory::Config::PagePolicy)utest;
      gem5->frontendLatency = utest;
      gem5->backendLatency = utest;
      gem5->maxAccessesPerRow = (uint32_t)utest;
      gem5->rowBufferSize = (uint32_t)utest;
      gem5->bankGroup = (uint32_t)utest;
      gem5->enablePowerdown = btest;
      gem5->useDLL = btest;

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
      reader.writeUint(Section::HostInterface, HIL::Config::Key::AXIWidth,
                       utest);
      reader.writeUint(Section::HostInterface, HIL::Config::Key::AXIClock,
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

      auto &disk = reader.getDiskList();
      disk.resize(1);
      disk.at(0).nsid = (uint32_t)utest;
      disk.at(0).enable = btest;
      disk.at(0).strict = btest;
      disk.at(0).useCoW = btest;
      disk.at(0).path = stest;

      auto &nslist = reader.getNamespaceList();
      nslist.resize(1);
      nslist.at(0).nsid = (uint32_t)utest;
      nslist.at(0).lbaSize = (uint16_t)utest;
      nslist.at(0).capacity = utest;

      // Section::InternalCache
      reader.writeBoolean(Section::InternalCache, ICL::Config::Key::EnableCache,
                          btest);
      reader.writeBoolean(Section::InternalCache,
                          ICL::Config::Key::EnablePrefetch, btest);
      reader.writeUint(Section::InternalCache, ICL::Config::Key::PrefetchMode,
                       utest);
      reader.writeUint(Section::InternalCache, ICL::Config::Key::PrefetchCount,
                       utest);
      reader.writeUint(Section::InternalCache, ICL::Config::Key::PrefetchRatio,
                       utest);
      reader.writeUint(Section::InternalCache, ICL::Config::Key::CacheMode,
                       utest);
      reader.writeUint(Section::InternalCache, ICL::Config::Key::CacheSize,
                       utest);
      reader.writeUint(Section::InternalCache, ICL::Config::Key::EvictPolicy,
                       utest);
      reader.writeUint(Section::InternalCache, ICL::Config::Key::EvictMode,
                       utest);
      reader.writeFloat(Section::InternalCache,
                        ICL::Config::Key::EvictThreshold, ftest);

      // Section::FlashTranslation
      reader.writeUint(Section::FlashTranslation, FTL::Config::Key::MappingMode,
                       utest);
      reader.writeFloat(Section::FlashTranslation,
                        FTL::Config::Key::OverProvisioningRatio, ftest);
      reader.writeUint(Section::FlashTranslation,
                       FTL::Config::Key::EraseThreshold, utest);
      reader.writeUint(Section::FlashTranslation, FTL::Config::Key::FillingMode,
                       utest);
      reader.writeFloat(Section::FlashTranslation, FTL::Config::Key::FillRatio,
                        ftest);
      reader.writeFloat(Section::FlashTranslation,
                        FTL::Config::Key::InvalidFillRatio, ftest);
      reader.writeUint(Section::FlashTranslation,
                       FTL::Config::Key::VictimSelectionPolicy, utest);
      reader.writeUint(Section::FlashTranslation,
                       FTL::Config::Key::DChoiceParam, utest);
      reader.writeFloat(Section::FlashTranslation,
                        FTL::Config::Key::GCThreshold, ftest);
      reader.writeUint(Section::FlashTranslation, FTL::Config::Key::GCMode,
                       utest);
      reader.writeUint(Section::FlashTranslation,
                       FTL::Config::Key::GCReclaimBlocks, utest);
      reader.writeFloat(Section::FlashTranslation,
                        FTL::Config::Key::GCReclaimThreshold, ftest);
      reader.writeBoolean(Section::FlashTranslation,
                          FTL::Config::Key::UseSuperpage, btest);
      reader.writeUint(Section::FlashTranslation,
                       FTL::Config::Key::SuperpageAllocation, utest);
      reader.writeFloat(Section::FlashTranslation,
                        FTL::Config::Key::VLTableRatio, ftest);
      reader.writeFloat(Section::FlashTranslation,
                        FTL::Config::Key::MergeBeginThreshold, ftest);
      reader.writeFloat(Section::FlashTranslation,
                        FTL::Config::Key::MergeEndThreshold, ftest);

      // Section::FlashInterface
      reader.writeUint(Section::FlashInterface, FIL::Config::Key::Channel,
                       utest);
      reader.writeUint(Section::FlashInterface, FIL::Config::Key::Way, utest);
      reader.writeUint(Section::FlashInterface, FIL::Config::Key::DMASpeed,
                       utest);
      reader.writeUint(Section::FlashInterface, FIL::Config::Key::DMAWidth,
                       utest);
      reader.writeUint(Section::FlashInterface, FIL::Config::Key::Model, utest);

      auto nand = reader.getNANDStructure();
      nand->type = (FIL::Config::NANDType)utest;
      nand->nop = (uint8_t)utest;
      nand->pageAllocation[0] = (FIL::PageAllocation)utest;
      nand->pageAllocation[1] = (FIL::PageAllocation)utest;
      nand->pageAllocation[2] = (FIL::PageAllocation)utest;
      nand->pageAllocation[3] = (FIL::PageAllocation)utest;
      nand->die = (uint32_t)utest;
      nand->plane = (uint32_t)utest;
      nand->block = (uint32_t)utest;
      nand->page = (uint32_t)utest;
      nand->pageSize = (uint32_t)utest;
      nand->spareSize = (uint32_t)utest;

      auto ntiming = reader.getNANDTiming();
      ntiming->tADL = (uint32_t)utest;
      ntiming->tCS = (uint32_t)utest;
      ntiming->tDH = (uint32_t)utest;
      ntiming->tDS = (uint32_t)utest;
      ntiming->tRC = (uint32_t)utest;
      ntiming->tRR = (uint32_t)utest;
      ntiming->tWB = (uint32_t)utest;
      ntiming->tWC = (uint32_t)utest;
      ntiming->tWP = (uint32_t)utest;
      ntiming->tCBSY = (uint32_t)utest;
      ntiming->tDBSY = (uint32_t)utest;
      ntiming->tRCBSY = (uint32_t)utest;
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

    // Write test
    reader.save(argv[1]);
  }

  {
    ConfigReader reader;

    reader.load(argv[1]);
    reader.save(argv[2]);
  }

  return 0;
}
