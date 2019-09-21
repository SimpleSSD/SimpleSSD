// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __CPU_CPU__
#define __CPU_CPU__

#include <cinttypes>
#include <queue>
#include <unordered_map>
#include <vector>

#include "cpu/def.hh"
#include "lib/mcpat/mcpat.h"
#include "sim/object.hh"

namespace SimpleSSD::CPU {

typedef struct _InstStat {
  // Instruction count
  uint64_t branch;
  uint64_t load;
  uint64_t store;
  uint64_t arithmetic;
  uint64_t floatingPoint;
  uint64_t otherInsts;

  // Total times in ps to execute this insturcion group
  uint64_t latency;

  _InstStat();
  _InstStat(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
            uint64_t);
  _InstStat &operator+=(const _InstStat &);
  uint64_t sum();
} InstStat;

typedef struct _JobEntry {
  EventFunction func;
  void *context;
  InstStat *inst;
  uint64_t submitAt;
  uint64_t delay;

  _JobEntry(EventFunction &, void *, InstStat *);
} JobEntry;

class CPU : public Object {
 private:
  struct CoreStat {
    InstStat instStat;
    uint64_t busy;

    CoreStat();
  };

  class Core {
   private:
    Engine *engine;
    bool busy;

    Event jobEvent;
    std::queue<JobEntry> jobs;

    CoreStat stat;

    void handleJob();
    void jobDone();

   public:
    Core(Engine *);
    ~Core();

    void submitJob(JobEntry, uint64_t = 0);

    void addStat(InstStat &);

    bool isBusy();
    uint64_t getJobListSize();
    CoreStat &getStat();
  };

  uint64_t lastResetStat;

  uint64_t clockSpeed;
  uint64_t clockPeriod;

  // Cores
  std::vector<Core> hilCore;
  std::vector<Core> iclCore;
  std::vector<Core> ftlCore;

  // CPIs
  std::unordered_map<Namespace, std::unordered_map<Function, InstStat>> cpi;

  uint32_t leastBusyCPU(std::vector<Core> &);
  void calculatePower(Power &);

 public:
  CPU(Engine *, ConfigReader *, Log *);
  ~CPU();

  void execute(Namespace, Function, EventFunction &, void * = nullptr,
               uint64_t = 0);
  uint64_t applyLatency(Namespace, Function);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint() noexcept override;
  void restoreCheckpoint() noexcept override;
};

}  // namespace SimpleSSD::CPU

#endif
