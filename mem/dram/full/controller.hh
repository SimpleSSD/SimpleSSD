// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_DRAM_SIMPLE_CONTROLLER_HH__
#define __SIMPLESSD_MEM_DRAM_SIMPLE_CONTROLLER_HH__

#include <functional>
#include <list>
#include <unordered_map>

#include "mem/def.hh"
#include "mem/dram/simple/rank.hh"

namespace SimpleSSD::Memory::DRAM::Simple {

class SimpleDRAM;

/**
 * \brief Simple memory controller
 *
 * Simple memory controller, with no scheduling methods (just FCFS).
 */
class Controller : public Object {
 private:
  SimpleDRAM *parent;

  // Request queue
  uint16_t requestDepth;
  uint16_t maxRequestDepth;
  uint16_t commandDepth;
  uint16_t maxCommandDepth;

  std::list<Packet *> requestQueue;
  std::list<Packet *> commandQueue;

  // Ranks
  std::vector<Rank> ranks;

  // Read completion queue
  std::unordered_map<uint64_t, Packet *> readCompletion;

  Event eventWork;

  void work(uint64_t);

 public:
  Controller(ObjectData &, SimpleDRAM *, Timing *);
  ~Controller();

  bool submit(Packet *) noexcept;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;

  Packet *restorePacket(Packet *) noexcept;
};

}  // namespace SimpleSSD::Memory::DRAM::Simple

#endif
