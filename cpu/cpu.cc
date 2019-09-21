// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "cpu/cpu.hh"

#include <limits>

namespace SimpleSSD::CPU {

InstStat::_InstStat()
    : branch(0),
      load(0),
      store(0),
      arithmetic(0),
      floatingPoint(0),
      otherInsts(0),
      latency(0) {}

InstStat::_InstStat(uint64_t b, uint64_t l, uint64_t s, uint64_t a, uint64_t f,
                    uint64_t o, uint64_t c)
    : branch(b),
      load(l),
      store(s),
      arithmetic(a),
      floatingPoint(f),
      otherInsts(o) {
  latency = sum();
  latency *= c;
}

InstStat &InstStat::operator+=(const InstStat &rhs) {
  this->branch += rhs.branch;
  this->load += rhs.load;
  this->store += rhs.store;
  this->arithmetic += rhs.arithmetic;
  this->floatingPoint += rhs.floatingPoint;
  this->otherInsts += rhs.otherInsts;
  // Do not add up totalcycles

  return *this;
}

uint64_t InstStat::sum() {
  return branch + load + store + arithmetic + floatingPoint + otherInsts;
}

JobEntry::_JobEntry(EventFunction &f, void *c, InstStat *i)
    : func(f), context(c), inst(i) {}

CPU::CoreStat::CoreStat() : busy(0) {}

CPU::Core::Core(Engine *e) : engine(e), busy(false) {
  jobEvent = engine->createEvent([this](uint64_t, void *) { jobDone(); },
                                 "CPU::Core::jobEvent");
}

CPU::Core::~Core() {}

bool CPU::CPU::Core::isBusy() {
  return busy;
}

uint64_t CPU::CPU::Core::getJobListSize() {
  return jobs.size();
}

CPU::CoreStat &CPU::Core::getStat() {
  return stat;
}

void CPU::Core::submitJob(JobEntry job, uint64_t delay) {
  job.delay = delay;
  job.submitAt = engine->getTick();
  jobs.push(job);

  if (!busy) {
    handleJob();
  }
}

void CPU::Core::handleJob() {
  auto &iter = jobs.front();
  uint64_t now = engine->getTick();
  uint64_t diff = now - iter.submitAt;
  uint64_t finishedAt;

  if (diff >= iter.delay) {
    finishedAt = now + iter.inst->latency;
  }
  else {
    finishedAt = now + iter.inst->latency + iter.delay - diff;
  }

  busy = true;

  engine->schedule(jobEvent, finishedAt);
}

void CPU::Core::jobDone() {
  auto &iter = jobs.front();

  iter.func(engine->getTick(), iter.context);
  stat.busy += iter.inst->latency;
  stat.instStat += *iter.inst;

  jobs.pop();
  busy = false;

  if (jobs.size() > 0) {
    handleJob();
  }
}

void CPU::Core::addStat(InstStat &inst) {
  stat.busy += inst.latency;
  stat.instStat += inst;
}

CPU::CPU(Engine *e, ConfigReader *c, Log *l)
    : Object(e, c, l), lastResetStat(0) {
  clockSpeed = readConfigUint(Section::CPU, Config::Key::Clock);
  clockPeriod = 1000000000000. / clockSpeed;  // in pico-seconds

  hilCore.resize(readConfigUint(Section::CPU, Config::Key::HILCore));
  iclCore.resize(readConfigUint(Section::CPU, Config::Key::ICLCore));
  ftlCore.resize(readConfigUint(Section::CPU, Config::Key::FTLCore));

  // Initialize CPU table
  cpi.insert({Namespace::FTL, std::unordered_map<Function, InstStat>()});
  cpi.insert(
      {Namespace::FTL__PAGE_MAPPING, std::unordered_map<Function, InstStat>()});
  cpi.insert({Namespace::ICL, std::unordered_map<Function, InstStat>()});
  cpi.insert({Namespace::ICL__GENERIC_CACHE,
              std::unordered_map<Function, InstStat>()});
  cpi.insert({Namespace::HIL, std::unordered_map<Function, InstStat>()});
  cpi.insert(
      {Namespace::NVME__CONTROLLER, std::unordered_map<Function, InstStat>()});
  cpi.insert(
      {Namespace::NVME__PRPLIST, std::unordered_map<Function, InstStat>()});
  cpi.insert({Namespace::NVME__SGL, std::unordered_map<Function, InstStat>()});
  cpi.insert(
      {Namespace::NVME__SUBSYSTEM, std::unordered_map<Function, InstStat>()});
  cpi.insert(
      {Namespace::NVME__NAMESPACE, std::unordered_map<Function, InstStat>()});
  cpi.insert(
      {Namespace::NVME__OCSSD, std::unordered_map<Function, InstStat>()});
  cpi.insert(
      {Namespace::UFS__DEVICE, std::unordered_map<Function, InstStat>()});
  cpi.insert(
      {Namespace::SATA__DEVICE, std::unordered_map<Function, InstStat>()});

  // Insert item (Use cpu/generator/generate.py to generate this code)
}

CPU::~CPU() {}

void CPU::calculatePower(Power &power) {
  // Print stats before die
  ParseXML param;
  uint64_t simCycle = (getTick() - lastResetStat) / clockPeriod;
  uint32_t totalCore = hilCore.size() + iclCore.size() + ftlCore.size();
  uint32_t coreIdx = 0;

  param.initialize();

  // system
  {
    param.sys.number_of_L1Directories = 0;
    param.sys.number_of_L2Directories = 0;
    param.sys.number_of_L2s = 1;
    param.sys.Private_L2 = 0;
    param.sys.number_of_L3s = 0;
    param.sys.number_of_NoCs = 0;
    param.sys.homogeneous_cores = 0;
    param.sys.homogeneous_L2s = 1;
    param.sys.homogeneous_L1Directories = 1;
    param.sys.homogeneous_L2Directories = 1;
    param.sys.homogeneous_L3s = 1;
    param.sys.homogeneous_ccs = 1;
    param.sys.homogeneous_NoCs = 1;
    param.sys.core_tech_node = 40;
    param.sys.target_core_clockrate = clockSpeed / 1000000;
    param.sys.temperature = 340;
    param.sys.number_cache_levels = 2;
    param.sys.interconnect_projection_type = 1;
    param.sys.device_type = 0;
    param.sys.longer_channel_device = 1;
    param.sys.Embedded = 1;
    param.sys.opt_clockrate = 1;
    param.sys.machine_bits = 64;
    param.sys.virtual_address_width = 48;
    param.sys.physical_address_width = 48;
    param.sys.virtual_memory_page_size = 4096;
    param.sys.total_cycles = simCycle;
    param.sys.number_of_cores = totalCore;
  }

  // Now, we are using heterogeneous cores
  for (coreIdx = 0; coreIdx < totalCore; coreIdx++) {
    // system.core
    {
      param.sys.core[coreIdx].clock_rate = param.sys.target_core_clockrate;
      param.sys.core[coreIdx].opt_local = 0;
      param.sys.core[coreIdx].instruction_length = 32;
      param.sys.core[coreIdx].opcode_width = 7;
      param.sys.core[coreIdx].x86 = 0;
      param.sys.core[coreIdx].micro_opcode_width = 8;
      param.sys.core[coreIdx].machine_type = 0;
      param.sys.core[coreIdx].number_hardware_threads = 1;
      param.sys.core[coreIdx].fetch_width = 2;
      param.sys.core[coreIdx].number_instruction_fetch_ports = 1;
      param.sys.core[coreIdx].decode_width = 2;
      param.sys.core[coreIdx].issue_width = 4;
      param.sys.core[coreIdx].peak_issue_width = 7;
      param.sys.core[coreIdx].commit_width = 4;
      param.sys.core[coreIdx].fp_issue_width = 1;
      param.sys.core[coreIdx].prediction_width = 0;
      param.sys.core[coreIdx].pipelines_per_core[0] = 1;
      param.sys.core[coreIdx].pipelines_per_core[1] = 1;
      param.sys.core[coreIdx].pipeline_depth[0] = 8;
      param.sys.core[coreIdx].pipeline_depth[1] = 8;
      param.sys.core[coreIdx].ALU_per_core = 3;
      param.sys.core[coreIdx].MUL_per_core = 1;
      param.sys.core[coreIdx].FPU_per_core = 1;
      param.sys.core[coreIdx].instruction_buffer_size = 32;
      param.sys.core[coreIdx].decoded_stream_buffer_size = 16;
      param.sys.core[coreIdx].instruction_window_scheme = 0;
      param.sys.core[coreIdx].instruction_window_size = 20;
      param.sys.core[coreIdx].fp_instruction_window_size = 15;
      param.sys.core[coreIdx].ROB_size = 0;
      param.sys.core[coreIdx].archi_Regs_IRF_size = 32;
      param.sys.core[coreIdx].archi_Regs_FRF_size = 32;
      param.sys.core[coreIdx].phy_Regs_IRF_size = 64;
      param.sys.core[coreIdx].phy_Regs_FRF_size = 64;
      param.sys.core[coreIdx].rename_scheme = 0;
      param.sys.core[coreIdx].checkpoint_depth = 1;
      param.sys.core[coreIdx].register_windows_size = 0;
      strcpy(param.sys.core[coreIdx].LSU_order, "inorder");
      param.sys.core[coreIdx].store_buffer_size = 4;
      param.sys.core[coreIdx].load_buffer_size = 0;
      param.sys.core[coreIdx].memory_ports = 1;
      param.sys.core[coreIdx].RAS_size = 0;
      param.sys.core[coreIdx].number_of_BPT = 2;
      param.sys.core[coreIdx].number_of_BTB = 2;
    }

    // system.core.itlb
    param.sys.core[coreIdx].itlb.number_entries = 64;

    // system.core.icache
    {
      param.sys.core[coreIdx].icache.icache_config[0] = 32768;
      param.sys.core[coreIdx].icache.icache_config[1] = 8;
      param.sys.core[coreIdx].icache.icache_config[2] = 4;
      param.sys.core[coreIdx].icache.icache_config[3] = 1;
      param.sys.core[coreIdx].icache.icache_config[4] = 10;
      param.sys.core[coreIdx].icache.icache_config[5] = 10;
      param.sys.core[coreIdx].icache.icache_config[6] = 32;
      param.sys.core[coreIdx].icache.icache_config[7] = 0;
      param.sys.core[coreIdx].icache.buffer_sizes[0] = 4;
      param.sys.core[coreIdx].icache.buffer_sizes[1] = 4;
      param.sys.core[coreIdx].icache.buffer_sizes[2] = 4;
      param.sys.core[coreIdx].icache.buffer_sizes[3] = 0;
    }

    // system.core dtlb
    param.sys.core[coreIdx].dtlb.number_entries = 64;

    // system.core.dcache
    {
      param.sys.core[coreIdx].dcache.dcache_config[0] = 32768;
      param.sys.core[coreIdx].dcache.dcache_config[1] = 8;
      param.sys.core[coreIdx].dcache.dcache_config[2] = 4;
      param.sys.core[coreIdx].dcache.dcache_config[3] = 1;
      param.sys.core[coreIdx].dcache.dcache_config[4] = 10;
      param.sys.core[coreIdx].dcache.dcache_config[5] = 10;
      param.sys.core[coreIdx].dcache.dcache_config[6] = 32;
      param.sys.core[coreIdx].dcache.dcache_config[7] = 0;
      param.sys.core[coreIdx].dcache.buffer_sizes[0] = 4;
      param.sys.core[coreIdx].dcache.buffer_sizes[1] = 4;
      param.sys.core[coreIdx].dcache.buffer_sizes[2] = 4;
      param.sys.core[coreIdx].dcache.buffer_sizes[3] = 4;
    }

    // system.core.BTB
    param.sys.core[coreIdx].BTB.BTB_config[0] = 4096;
    param.sys.core[coreIdx].BTB.BTB_config[1] = 4;
    param.sys.core[coreIdx].BTB.BTB_config[2] = 2;
    param.sys.core[coreIdx].BTB.BTB_config[3] = 2;
    param.sys.core[coreIdx].BTB.BTB_config[4] = 1;
    param.sys.core[coreIdx].BTB.BTB_config[5] = 1;
  }

  // system.L2
  {
    param.sys.L2[0].L2_config[0] = 1048576;
    param.sys.L2[0].L2_config[1] = 32;
    param.sys.L2[0].L2_config[2] = 8;
    param.sys.L2[0].L2_config[3] = 8;
    param.sys.L2[0].L2_config[4] = 8;
    param.sys.L2[0].L2_config[5] = 23;
    param.sys.L2[0].L2_config[6] = 32;
    param.sys.L2[0].L2_config[7] = 1;
    param.sys.L2[0].buffer_sizes[0] = 16;
    param.sys.L2[0].buffer_sizes[1] = 16;
    param.sys.L2[0].buffer_sizes[2] = 16;
    param.sys.L2[0].buffer_sizes[3] = 16;
    param.sys.L2[0].clockrate = param.sys.target_core_clockrate;
    param.sys.L2[0].ports[0] = 1;
    param.sys.L2[0].ports[1] = 1;
    param.sys.L2[0].ports[2] = 1;
    param.sys.L2[0].device_type = 0;
  }

  // Etcetera
  param.sys.mc.req_window_size_per_channel = 32;

  // Apply stat values

  // Core stat
  coreIdx = 0;

  for (auto &core : hilCore) {
    CoreStat &stat = core.getStat();

    param.sys.core[coreIdx].total_instructions = stat.instStat.sum();
    param.sys.core[coreIdx].int_instructions = stat.instStat.arithmetic;
    param.sys.core[coreIdx].fp_instructions = stat.instStat.floatingPoint;
    param.sys.core[coreIdx].branch_instructions = stat.instStat.branch;
    param.sys.core[coreIdx].load_instructions = stat.instStat.load;
    param.sys.core[coreIdx].store_instructions = stat.instStat.store;
    param.sys.core[coreIdx].busy_cycles = stat.busy / clockPeriod;

    coreIdx++;
  }

  for (auto &core : iclCore) {
    CoreStat &stat = core.getStat();

    param.sys.core[coreIdx].total_instructions = stat.instStat.sum();
    param.sys.core[coreIdx].int_instructions = stat.instStat.arithmetic;
    param.sys.core[coreIdx].fp_instructions = stat.instStat.floatingPoint;
    param.sys.core[coreIdx].branch_instructions = stat.instStat.branch;
    param.sys.core[coreIdx].load_instructions = stat.instStat.load;
    param.sys.core[coreIdx].store_instructions = stat.instStat.store;
    param.sys.core[coreIdx].busy_cycles = stat.busy / clockPeriod;

    coreIdx++;
  }

  for (auto &core : ftlCore) {
    CoreStat &stat = core.getStat();

    param.sys.core[coreIdx].total_instructions = stat.instStat.sum();
    param.sys.core[coreIdx].int_instructions = stat.instStat.arithmetic;
    param.sys.core[coreIdx].fp_instructions = stat.instStat.floatingPoint;
    param.sys.core[coreIdx].branch_instructions = stat.instStat.branch;
    param.sys.core[coreIdx].load_instructions = stat.instStat.load;
    param.sys.core[coreIdx].store_instructions = stat.instStat.store;
    param.sys.core[coreIdx].busy_cycles = stat.busy / clockPeriod;

    coreIdx++;
  };

  for (coreIdx = 0; coreIdx < totalCore; coreIdx++) {
    param.sys.core[coreIdx].total_cycles = simCycle;
    param.sys.core[coreIdx].idle_cycles =
        simCycle - param.sys.core[coreIdx].busy_cycles;
    param.sys.core[coreIdx].committed_instructions =
        param.sys.core[coreIdx].total_instructions;
    param.sys.core[coreIdx].committed_int_instructions =
        param.sys.core[coreIdx].int_instructions;
    param.sys.core[coreIdx].committed_fp_instructions =
        param.sys.core[coreIdx].fp_instructions;
    param.sys.core[coreIdx].pipeline_duty_cycle = 1;
    param.sys.core[coreIdx].IFU_duty_cycle = 0.9;
    param.sys.core[coreIdx].BR_duty_cycle = 0.72;
    param.sys.core[coreIdx].LSU_duty_cycle = 0.71;
    param.sys.core[coreIdx].MemManU_I_duty_cycle = 0.9;
    param.sys.core[coreIdx].MemManU_D_duty_cycle = 0.71;
    param.sys.core[coreIdx].ALU_duty_cycle = 0.76;
    param.sys.core[coreIdx].MUL_duty_cycle = 0.82;
    param.sys.core[coreIdx].FPU_duty_cycle = 0.0;
    param.sys.core[coreIdx].ALU_cdb_duty_cycle = 0.76;
    param.sys.core[coreIdx].MUL_cdb_duty_cycle = 0.82;
    param.sys.core[coreIdx].FPU_cdb_duty_cycle = 0.0;
    param.sys.core[coreIdx].ialu_accesses = param.sys.core[0].int_instructions;
    param.sys.core[coreIdx].fpu_accesses = param.sys.core[0].fp_instructions;
    param.sys.core[coreIdx].mul_accesses =
        param.sys.core[0].int_instructions * .5;
    param.sys.core[coreIdx].int_regfile_reads =
        param.sys.core[0].load_instructions;
    param.sys.core[coreIdx].float_regfile_reads =
        param.sys.core[coreIdx].fp_instructions * .4;
    param.sys.core[coreIdx].int_regfile_writes =
        param.sys.core[0].store_instructions;
    param.sys.core[coreIdx].float_regfile_writes =
        param.sys.core[coreIdx].fp_instructions * .4;

    // L1i and L1d
    {
      auto &core = param.sys.core[coreIdx];

      core.icache.total_accesses = core.load_instructions * .3;
      core.icache.total_hits = core.icache.total_accesses * .7;
      core.icache.total_misses = core.icache.total_accesses * .3;
      core.icache.read_accesses = core.icache.total_accesses;
      core.icache.read_hits = core.icache.total_hits;
      core.icache.read_misses = core.icache.total_misses;
      core.itlb.total_accesses = core.load_instructions * .2;
      core.itlb.total_hits = core.itlb.total_accesses * .8;
      core.itlb.total_misses = core.itlb.total_accesses * .2;

      core.dcache.total_accesses = core.load_instructions * .4;
      core.dcache.total_hits = core.dcache.total_accesses * .4;
      core.dcache.total_misses = core.dcache.total_accesses * .6;
      core.dcache.read_accesses = core.dcache.total_accesses * .6;
      core.dcache.read_hits = core.dcache.total_hits * .6;
      core.dcache.read_misses = core.dcache.total_misses * .6;
      core.dcache.write_accesses = core.dcache.total_accesses * .4;
      core.dcache.write_hits = core.dcache.total_hits * .4;
      core.dcache.write_misses = core.dcache.total_misses * .4;
      core.dcache.write_backs = core.dcache.total_misses * .4;
      core.dtlb.total_accesses = core.load_instructions * .2;
      core.dtlb.total_hits = core.itlb.total_accesses * .8;
      core.dtlb.total_misses = core.itlb.total_accesses * .2;
    }
  }

  // L2
  param.sys.L2[0].duty_cycle = 1.;

  if (param.sys.number_of_L2s > 0) {
    auto &L2 = param.sys.L2[0];

    L2.total_accesses = param.sys.core[0].dcache.write_backs;
    L2.total_hits = L2.total_accesses * .4;
    L2.total_misses = L2.total_accesses * .6;
    L2.read_accesses = L2.total_accesses * .5;
    L2.read_hits = L2.total_hits * .5;
    L2.read_misses = L2.total_misses * .5;
    L2.write_accesses = L2.total_accesses * .5;
    L2.write_hits = L2.total_hits * .5;
    L2.write_misses = L2.total_misses * .5;
    L2.write_backs = L2.total_misses * .4;
  }
  else {
    param.sys.L2[0].total_accesses = 0;
    param.sys.L2[0].total_hits = 0;
    param.sys.L2[0].total_misses = 0;
    param.sys.L2[0].read_accesses = 0;
    param.sys.L2[0].read_hits = 0;
    param.sys.L2[0].read_misses = 0;
    param.sys.L2[0].write_accesses = 0;
    param.sys.L2[0].write_hits = 0;
    param.sys.L2[0].write_misses = 0;
    param.sys.L2[0].write_backs = 0;
  }

  // L3
  param.sys.L3[0].duty_cycle = 1.;

  if (param.sys.number_of_L3s > 0) {
    auto &L3 = param.sys.L3[0];

    L3.total_accesses = param.sys.L2[0].write_backs;
    L3.total_hits = L3.total_accesses * .4;
    L3.total_misses = L3.total_accesses * .6;
    L3.read_accesses = L3.total_accesses * .5;
    L3.read_hits = L3.total_hits * .5;
    L3.read_misses = L3.total_misses * .5;
    L3.write_accesses = L3.total_accesses * .5;
    L3.write_hits = L3.total_hits * .5;
    L3.write_misses = L3.total_misses * .5;
    L3.write_backs = L3.total_misses * .4;
  }
  else {
    param.sys.L3[0].total_accesses = 0;
    param.sys.L3[0].total_hits = 0;
    param.sys.L3[0].total_misses = 0;
    param.sys.L3[0].read_accesses = 0;
    param.sys.L3[0].read_hits = 0;
    param.sys.L3[0].read_misses = 0;
    param.sys.L3[0].write_accesses = 0;
    param.sys.L3[0].write_hits = 0;
    param.sys.L3[0].write_misses = 0;
    param.sys.L3[0].write_backs = 0;
  }

  McPAT mcpat(&param);

  mcpat.getPower(power);
}

uint32_t CPU::leastBusyCPU(std::vector<Core> &list) {
  uint32_t idx = list.size();
  uint64_t busymin = std::numeric_limits<uint64_t>::max();

  for (uint32_t i = 0; i < list.size(); i++) {
    auto &core = list.at(i);
    auto busy = core.getStat().busy;

    if (!core.isBusy() && busy < busymin) {
      busymin = busy;
      idx = i;
    }
  }

  if (idx == list.size()) {
    uint64_t jobmin = std::numeric_limits<uint64_t>::max();

    for (uint32_t i = 0; i < list.size(); i++) {
      auto jobs = list.at(i).getJobListSize();

      if (jobs <= jobmin) {
        jobmin = jobs;
        idx = i;
      }
    }
  }

  return idx;
}

void CPU::execute(Namespace ns, Function fct, EventFunction &func,
                  void *context, uint64_t delay) {
  Core *pCore = nullptr;

  // Find dedicated core
  switch (ns) {
    case Namespace::FTL:
    case Namespace::FTL__PAGE_MAPPING:
      if (ftlCore.size() > 0) {
        pCore = &ftlCore.at(leastBusyCPU(ftlCore));
      }

      break;
    case Namespace::ICL:
    case Namespace::ICL__GENERIC_CACHE:
      if (iclCore.size() > 0) {
        pCore = &iclCore.at(leastBusyCPU(iclCore));
      }

      break;
    case Namespace::HIL:
    case Namespace::NVME__CONTROLLER:
    case Namespace::NVME__PRPLIST:
    case Namespace::NVME__SGL:
    case Namespace::NVME__SUBSYSTEM:
    case Namespace::NVME__NAMESPACE:
    case Namespace::NVME__OCSSD:
    case Namespace::UFS__DEVICE:
    case Namespace::SATA__DEVICE:
      if (hilCore.size() > 0) {
        pCore = &hilCore.at(leastBusyCPU(hilCore));
      }

      break;
    default:
      panic("Undefined function namespace %u", ns);

      break;
  }

  if (pCore) {
    // Get CPI table
    auto table = cpi.find(ns);

    if (table == cpi.end()) {
      panic("Namespace %u not defined in CPI table", ns);
    }

    // Get CPI
    auto inst = table->second.find(fct);

    if (inst == table->second.end()) {
      panic("Namespace %u does not have function %u", ns, fct);
    }

    pCore->submitJob(JobEntry(func, context, &inst->second), delay);
  }
  else {
    func(getTick(), context);
  }
}

uint64_t CPU::applyLatency(Namespace ns, Function fct) {
  Core *pCore = nullptr;

  // Find dedicated core
  switch (ns) {
    case Namespace::FTL:
    case Namespace::FTL__PAGE_MAPPING:
      if (ftlCore.size() > 0) {
        pCore = &ftlCore.at(leastBusyCPU(ftlCore));
      }

      break;
    case Namespace::ICL:
    case Namespace::ICL__GENERIC_CACHE:
      if (iclCore.size() > 0) {
        pCore = &iclCore.at(leastBusyCPU(iclCore));
      }

      break;
    case Namespace::HIL:
    case Namespace::NVME__CONTROLLER:
    case Namespace::NVME__PRPLIST:
    case Namespace::NVME__SGL:
    case Namespace::NVME__SUBSYSTEM:
    case Namespace::NVME__NAMESPACE:
    case Namespace::NVME__OCSSD:
    case Namespace::UFS__DEVICE:
    case Namespace::SATA__DEVICE:
      if (hilCore.size() > 0) {
        pCore = &hilCore.at(leastBusyCPU(hilCore));
      }

      break;
    default:
      panic("Undefined function namespace %u", ns);

      break;
  }

  if (pCore) {
    // Get CPI table
    auto table = cpi.find(ns);

    if (table == cpi.end()) {
      panic("Namespace %u not defined in CPI table", ns);
    }

    // Get CPI
    auto inst = table->second.find(fct);

    if (inst == table->second.end()) {
      panic("Namespace %u does not have function %u", ns, fct);
    }

    pCore->addStat(inst->second);

    return inst->second.latency;
  }

  return 0;
}

void CPU::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  Stat temp;
  std::string number;

  for (uint32_t i = 0; i < hilCore.size(); i++) {
    number = std::to_string(i);

    temp.name = prefix + ".hil" + number + ".busy";
    temp.desc = "CPU for HIL core " + number + " busy ticks";
    list.push_back(temp);

    temp.name = prefix + ".hil" + number + ".insts.branch";
    temp.desc = "CPU for HIL core " + number + " executed branch instructions";
    list.push_back(temp);

    temp.name = prefix + ".hil" + number + ".insts.load";
    temp.desc = "CPU for HIL core " + number + " executed load instructions";
    list.push_back(temp);

    temp.name = prefix + ".hil" + number + ".insts.store";
    temp.desc = "CPU for HIL core " + number + " executed store instructions";
    list.push_back(temp);

    temp.name = prefix + ".hil" + number + ".insts.arithmetic";
    temp.desc =
        "CPU for HIL core " + number + " executed arithmetic instructions";
    list.push_back(temp);

    temp.name = prefix + ".hil" + number + ".insts.fp";
    temp.desc =
        "CPU for HIL core " + number + " executed floating point instructions";
    list.push_back(temp);

    temp.name = prefix + ".hil" + number + ".insts.others";
    temp.desc = "CPU for HIL core " + number + " executed other instructions";
    list.push_back(temp);
  }

  for (uint32_t i = 0; i < iclCore.size(); i++) {
    number = std::to_string(i);

    temp.name = prefix + ".icl" + number + ".busy";
    temp.desc = "CPU for ICL core " + number + " busy ticks";
    list.push_back(temp);

    temp.name = prefix + ".icl" + number + ".insts.branch";
    temp.desc = "CPU for ICL core " + number + " executed branch instructions";
    list.push_back(temp);

    temp.name = prefix + ".icl" + number + ".insts.load";
    temp.desc = "CPU for ICL core " + number + " executed load instructions";
    list.push_back(temp);

    temp.name = prefix + ".icl" + number + ".insts.store";
    temp.desc = "CPU for ICL core " + number + " executed store instructions";
    list.push_back(temp);

    temp.name = prefix + ".icl" + number + ".insts.arithmetic";
    temp.desc =
        "CPU for ICL core " + number + " executed arithmetic instructions";
    list.push_back(temp);

    temp.name = prefix + ".icl" + number + ".insts.fp";
    temp.desc =
        "CPU for ICL core " + number + " executed floating point instructions";
    list.push_back(temp);

    temp.name = prefix + ".icl" + number + ".insts.others";
    temp.desc = "CPU for ICL core " + number + " executed other instructions";
    list.push_back(temp);
  }

  for (uint32_t i = 0; i < ftlCore.size(); i++) {
    number = std::to_string(i);

    temp.name = prefix + ".ftl" + number + ".busy";
    temp.desc = "CPU for FTL core " + number + " busy ticks";
    list.push_back(temp);

    temp.name = prefix + ".ftl" + number + ".insts.branch";
    temp.desc = "CPU for FTL core " + number + " executed branch instructions";
    list.push_back(temp);

    temp.name = prefix + ".ftl" + number + ".insts.load";
    temp.desc = "CPU for FTL core " + number + " executed store instructions";
    list.push_back(temp);

    temp.name = prefix + ".ftl" + number + ".insts.store";
    temp.desc = "CPU for FTL core " + number + " executed load instructions";
    list.push_back(temp);

    temp.name = prefix + ".ftl" + number + ".insts.arithmetic";
    temp.desc =
        "CPU for FTL core " + number + " executed arithmetic instructions";
    list.push_back(temp);

    temp.name = prefix + ".ftl" + number + ".insts.fp";
    temp.desc =
        "CPU for FTL core " + number + " executed floating point instructions";
    list.push_back(temp);

    temp.name = prefix + ".ftl" + number + ".insts.others";
    temp.desc = "CPU for FTL core " + number + " executed other instructions";
    list.push_back(temp);
  }
}

void CPU::getStatValues(std::vector<double> &values) noexcept {
  for (auto &core : hilCore) {
    auto &stat = core.getStat();

    values.push_back(stat.busy);
    values.push_back(stat.instStat.branch);
    values.push_back(stat.instStat.load);
    values.push_back(stat.instStat.store);
    values.push_back(stat.instStat.arithmetic);
    values.push_back(stat.instStat.floatingPoint);
    values.push_back(stat.instStat.otherInsts);
  }

  for (auto &core : iclCore) {
    auto &stat = core.getStat();

    values.push_back(stat.busy);
    values.push_back(stat.instStat.branch);
    values.push_back(stat.instStat.load);
    values.push_back(stat.instStat.store);
    values.push_back(stat.instStat.arithmetic);
    values.push_back(stat.instStat.floatingPoint);
    values.push_back(stat.instStat.otherInsts);
  }

  for (auto &core : ftlCore) {
    auto &stat = core.getStat();

    values.push_back(stat.busy);
    values.push_back(stat.instStat.branch);
    values.push_back(stat.instStat.load);
    values.push_back(stat.instStat.store);
    values.push_back(stat.instStat.arithmetic);
    values.push_back(stat.instStat.floatingPoint);
    values.push_back(stat.instStat.otherInsts);
  }
}

void CPU::resetStatValues() noexcept {
  lastResetStat = getTick();

  for (auto &core : hilCore) {
    auto &stat = core.getStat();

    stat.busy = 0;
    stat.instStat = InstStat();
  }

  for (auto &core : iclCore) {
    auto &stat = core.getStat();

    stat.busy = 0;
    stat.instStat = InstStat();
  }

  for (auto &core : ftlCore) {
    auto &stat = core.getStat();

    stat.busy = 0;
    stat.instStat = InstStat();
  }
}

// TODO: Add checkpointing API

}  // namespace SimpleSSD::CPU
