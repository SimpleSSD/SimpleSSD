// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "cpu/cpu.hh"

#include "sim/log.hh"

namespace SimpleSSD::CPU {

Function::Function()
    : branch(0),
      load(0),
      store(0),
      arithmetic(0),
      floatingPoint(0),
      otherInsts(0),
      cycles(0) {}

Function::Function(uint64_t b, uint64_t l, uint64_t s, uint64_t a, uint64_t f,
                   uint64_t o, uint64_t c)
    : branch(b),
      load(l),
      store(s),
      arithmetic(a),
      floatingPoint(f),
      otherInsts(o),
      cycles(c) {}

Function &Function::operator+=(const Function &rhs) {
  this->branch += rhs.branch;
  this->load += rhs.load;
  this->store += rhs.store;
  this->arithmetic += rhs.arithmetic;
  this->floatingPoint += rhs.floatingPoint;
  this->otherInsts += rhs.otherInsts;
  this->cycles += rhs.cycles;

  return *this;
}

uint64_t Function::sum() {
  return branch + load + store + arithmetic + floatingPoint + otherInsts;
}

void Function::clear() {
  branch = 0;
  load = 0;
  store = 0;
  arithmetic = 0;
  floatingPoint = 0;
  otherInsts = 0;
  cycles = 0;
}

Function initFunction() {
  return Function();
}

CPU::CPU(Engine *e, ConfigReader *c, Log *l)
    : engine(e), config(c), log(l), lastResetStat(0), lastScheduledAt(0) {
  clockSpeed = config->readUint(Section::CPU, Config::Key::Clock);
  clockPeriod = 1000000000000. / clockSpeed;  // in pico-seconds

  useDedicatedCore =
      config->readBoolean(Section::CPU, Config::Key::UseDedicatedCore);
  hilCore = config->readUint(Section::CPU, Config::Key::HILCore);
  iclCore = config->readUint(Section::CPU, Config::Key::ICLCore);
  ftlCore = config->readUint(Section::CPU, Config::Key::FTLCore);

  uint16_t totalCore = hilCore + iclCore + ftlCore;

  coreList.resize(totalCore);

  engine->setFunction(
      [this](uint64_t tick, uint64_t) { dispatch(tick); },
      [this](Event eid, uint64_t tick) { interrupt(eid, tick); });
}

CPU::~CPU() {
  // Delete all events
  for (auto &iter : eventList) {
    delete iter;
  }

  eventList.clear();
}

inline void CPU::panic_log(const char *format, ...) noexcept {
  va_list args;

  va_start(args, format);
  log->print(Log::LogID::Panic, format, args);
  va_end(args);
}

inline void CPU::warn_log(const char *format, ...) noexcept {
  va_list args;

  va_start(args, format);
  log->print(Log::LogID::Warn, format, args);
  va_end(args);
}

void CPU::calculatePower(Power &power) {
  // Print stats before die
  ParseXML param;
  uint64_t simCycle = (getTick() - lastResetStat) / clockPeriod;
  uint32_t totalCore = hilCore + iclCore + ftlCore;
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

  for (auto &core : coreList) {
    param.sys.core[coreIdx].total_instructions = core.instructionStat.sum();
    param.sys.core[coreIdx].int_instructions = core.instructionStat.arithmetic;
    param.sys.core[coreIdx].fp_instructions =
        core.instructionStat.floatingPoint;
    param.sys.core[coreIdx].branch_instructions = core.instructionStat.branch;
    param.sys.core[coreIdx].load_instructions = core.instructionStat.load;
    param.sys.core[coreIdx].store_instructions = core.instructionStat.store;
    param.sys.core[coreIdx].busy_cycles = core.eventStat.busy / clockPeriod;

    coreIdx++;
  }

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

CPU::Core *CPU::getIdleCoreInRange(uint16_t begin, uint16_t end) {
  Core *ret = nullptr;
  uint64_t minBusy = std::numeric_limits<uint64_t>::max();

  for (uint16_t i = begin; i < end; i++) {
    auto *core = &coreList.at(i);

    if (minBusy > core->busyUntil) {
      minBusy = core->busyUntil;
      ret = core;
    }
  }

  return ret;
}

void CPU::dispatch(uint64_t now) {
  // We are de-scheduled
  lastScheduledAt = std::numeric_limits<uint64_t>::max();

  // Find event to dispatch
  for (auto job = jobQueue.begin(); job != jobQueue.end();) {
    if (job->scheduledAt <= now) {
      EventData *eptr = job->eid;
      uint64_t data = job->data;

      job = jobQueue.erase(job);

      eptr->func(now, data);
    }
    else {
      break;
    }
  }

  scheduleNext();
}

void CPU::interrupt(Event eid, uint64_t tick) {
  // Engine invokes event!
  eid->func(tick, 0);
}

void CPU::scheduleNext() {
  // Schedule CPU
  uint64_t next = std::numeric_limits<uint64_t>::max();

  // Find event to dispatch
  auto job = jobQueue.begin();

  if (job != jobQueue.end()) {
    next = MIN(next, job->scheduledAt);
  }

  // Schedule next dispatch
  if (next != lastScheduledAt && next != std::numeric_limits<uint64_t>::max()) {
    lastScheduledAt = next;

    engine->schedule(next);
  }
}

uint64_t CPU::getTick() noexcept {
  return engine->getTick();
}

Event CPU::createEvent(const EventFunction &func,
                       const std::string &name) noexcept {
  Event eid = new EventData(std::move(func), std::move(name));

  if (UNLIKELY(engine->getTick() != 0)) {
    panic_log("All Event should be created in constructor (time = 0).");
  }

  eventList.push_back(eid);

  return eid;
}

void CPU::schedule(CPUGroup group, Event eid, uint64_t data,
                   const Function &func) noexcept {
  uint16_t begin = 0;
  uint16_t end = hilCore;
  uint64_t curTick = engine->getTick();

  if (UNLIKELY(eid == InvalidEventID)) {
    return;
  }

  if (UNLIKELY(func.cycles == 0)) {
    panic_log("Invalid function object passed.");
  }

  // Determine core number range
  switch (group) {
    case CPUGroup::HostInterface:
      break;
    case CPUGroup::InternalCache:
      begin = hilCore;
      end = hilCore + iclCore;
      break;
    case CPUGroup::FlashTranslationLayer:
      begin = hilCore + iclCore;
      /* fallthrough */
    case CPUGroup::Any:
      end = hilCore + iclCore + ftlCore;
      break;
  }

  auto core = getIdleCoreInRange(begin, end);

  if (UNLIKELY(!core)) {
    panic_log("Unexpected null-pointer while schedule.");
  }

  // Maybe core has running job
  uint64_t beginAt = MAX(curTick, core->busyUntil);

  // Current job runs after previous job
  uint64_t busy = func.cycles * clockPeriod;

  core->busyUntil = beginAt + busy;
  core->eventStat.busy += busy;
  core->eventStat.handledFunction++;

  scheduleAbs(eid, data, core->busyUntil);
}

void CPU::schedule(Event eid, uint64_t data, uint64_t delay) noexcept {
  scheduleAbs(eid, data, delay + engine->getTick());
}

void CPU::scheduleAbs(Event eid, uint64_t data, uint64_t tick) noexcept {
  if (UNLIKELY(tick < engine->getTick())) {
    panic_log("Invalid tick %" PRIu64, tick);
  }

  auto insert = jobQueue.end();

  for (auto entry = jobQueue.begin(); entry != jobQueue.end(); ++entry) {
    if (entry->scheduledAt > tick) {
      insert = entry;

      break;
    }
  }

  jobQueue.emplace(insert, Job(eid, data, tick));

  scheduleNext();
}

void CPU::deschedule(Event eid) noexcept {
  for (auto iter = jobQueue.begin(); iter != jobQueue.end(); ++iter) {
    if (iter->eid == eid) {
      jobQueue.erase(iter);

      return;
    }
  }
}

bool CPU::isScheduled(Event eid) noexcept {
  for (auto &iter : jobQueue) {
    if (iter.eid == eid) {
      return true;
    }
  }

  return false;
}

uint64_t CPU::when(Event eid) noexcept {
  for (auto &iter : jobQueue) {
    if (iter.eid == eid) {
      return iter.scheduledAt;
    }
  }

  return std::numeric_limits<uint64_t>::max();
}

void CPU::destroyEvent(Event) noexcept {
  panic_log("Not allowed to destroy event");
}

void CPU::getStatList(std::vector<Stat> &list, std::string prefix) noexcept {
  Stat temp;
  std::string number;
  std::string prefix2;
  uint32_t totalCore = hilCore + iclCore + ftlCore;

  if (useDedicatedCore) {
    prefix2 = ".hil";
  }
  else {
    prefix2 = ".core";
  }

  for (uint32_t i = 0; i < totalCore; i++) {
    number = std::to_string(i);

    if (useDedicatedCore && i == hilCore) {
      prefix2 = ".icl";
    }
    else if (useDedicatedCore && i == hilCore + iclCore) {
      prefix2 = ".ftl";
    }

    temp.name = prefix + prefix2 + number + ".busy";
    temp.desc = "CPU core " + number + " busy ticks";
    list.push_back(temp);

    temp.name = prefix + prefix2 + number + ".handled_function";
    temp.desc = "CPU core " + number + " total functions executed";
    list.push_back(temp);

    temp.name = prefix + prefix2 + number + ".insts.branch";
    temp.desc = "CPU core " + number + " executed branch instructions";
    list.push_back(temp);

    temp.name = prefix + prefix2 + number + ".insts.load";
    temp.desc = "CPU core " + number + " executed load instructions";
    list.push_back(temp);

    temp.name = prefix + prefix2 + number + ".insts.store";
    temp.desc = "CPU core " + number + " executed store instructions";
    list.push_back(temp);

    temp.name = prefix + prefix2 + number + ".insts.arithmetic";
    temp.desc = "CPU core " + number + " executed arithmetic instructions";
    list.push_back(temp);

    temp.name = prefix + prefix2 + number + ".insts.fp";
    temp.desc = "CPU core " + number + " executed floating point instructions";
    list.push_back(temp);

    temp.name = prefix + prefix2 + number + ".insts.others";
    temp.desc = "CPU core " + number + " executed other instructions";
    list.push_back(temp);
  }
}

void CPU::getStatValues(std::vector<double> &values) noexcept {
  for (auto &core : coreList) {
    values.push_back(core.eventStat.busy);
    values.push_back(core.eventStat.handledFunction);
    values.push_back(core.instructionStat.branch);
    values.push_back(core.instructionStat.load);
    values.push_back(core.instructionStat.store);
    values.push_back(core.instructionStat.arithmetic);
    values.push_back(core.instructionStat.floatingPoint);
    values.push_back(core.instructionStat.otherInsts);
  }
}

void CPU::resetStatValues() noexcept {
  lastResetStat = getTick();

  for (auto &core : coreList) {
    core.eventStat.clear();
    core.instructionStat.clear();
  }
}

void CPU::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, lastResetStat);
  BACKUP_SCALAR(out, lastScheduledAt);
  BACKUP_SCALAR(out, clockSpeed);
  BACKUP_SCALAR(out, clockPeriod);
  BACKUP_SCALAR(out, useDedicatedCore);
  BACKUP_SCALAR(out, hilCore);
  BACKUP_SCALAR(out, iclCore);
  BACKUP_SCALAR(out, ftlCore);

  uint64_t size = eventList.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : eventList) {
    BACKUP_SCALAR(out, iter);
  }

  size = jobQueue.size();
  BACKUP_SCALAR(out, size);

  for (auto &job : jobQueue) {
    BACKUP_EVENT(out, job.eid);
    BACKUP_SCALAR(out, job.data);
    BACKUP_SCALAR(out, job.scheduledAt);
  }
}

void CPU::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, lastResetStat);
  RESTORE_SCALAR(in, lastScheduledAt);
  RESTORE_SCALAR(in, clockSpeed);
  RESTORE_SCALAR(in, clockPeriod);
  RESTORE_SCALAR(in, useDedicatedCore);
  RESTORE_SCALAR(in, hilCore);
  RESTORE_SCALAR(in, iclCore);
  RESTORE_SCALAR(in, ftlCore);

  uint64_t size;
  Event eid;

  RESTORE_SCALAR(in, size);

  if (UNLIKELY(size != eventList.size())) {
    panic_log("Event count mismatch while restore CPU.");
  }

  // We must have exactly same event list
  for (uint64_t i = 0; i < size; i++) {
    RESTORE_SCALAR(in, eid);

    oldEventList.emplace(std::make_pair(eid, eventList[i]));
  }

  RESTORE_SCALAR(in, size);

  for (uint64_t i = 0; i < size; i++) {
    Event eid;
    uint64_t data;
    uint64_t scheduledAt;

    RESTORE_SCALAR(in, eid);
    eid = restoreEventID(eid);

    RESTORE_SCALAR(in, data);
    RESTORE_SCALAR(in, scheduledAt);

    jobQueue.emplace_back(Job(eid, data, scheduledAt));
  }
}

Event CPU::restoreEventID(Event old) noexcept {
  auto iter = oldEventList.find(old);

  if (iter == oldEventList.end()) {
    panic_log("Event not found");
  }

  return iter->second;
}

void CPU::clearOldEventList() noexcept {
  oldEventList.clear();
}

}  // namespace SimpleSSD::CPU
