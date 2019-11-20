// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_DRAM_DRAM_CONTROLLER_HH__
#define __SIMPLESSD_MEM_DRAM_DRAM_CONTROLLER_HH__

#include <unordered_set>

#include "sim/object.hh"

namespace SimpleSSD::Memory::DRAM {

class DRAMController;
class AbstractDRAM;

union Address {
  uint64_t data;
  struct {
    uint32_t row;
    uint8_t bank;
    uint8_t channel;
    uint16_t rank;
  };

  Address() : data(0) {}
  Address(uint64_t a) : data(a) {}
  Address(uint8_t c, uint16_t r, uint8_t b, uint32_t ro)
      : row(ro), bank(b), channel(c), rank(r) {}
};

class Channel : public Object {
 private:
  const uint8_t id;

  DRAMController *parent;
  Config::DRAMController *ctrl;
  AbstractDRAM *pDRAM;

  uint32_t entrySize;

  // Address matcher
  std::unordered_set<uint64_t> writeQueue;

  // Request queue
  struct Entry {
    uint64_t address;
    Event event;
    uint64_t data;

    Entry(uint64_t a, Event e, uint64_t d) : address(a), event(e), data(d) {}
  };

  std::list<Entry> readRequestQueue;
  std::list<Entry> writeRequestQueue;

  std::function<Address(uint64_t)> &decodeAddress;

  uint8_t addToReadQueue(uint64_t, Event, uint64_t);
  uint8_t addToWriteQueue(uint64_t);

  // Scheduler
  std::function<std::list<Entry>::iterator(std::list<Entry> &)> chooseNext;

  // Submission
  bool isInRead;
  uint32_t writeCountInWrite;

  void submitRequest();

  Event eventDoNext;

  // Completion handler
  Event eventReadDone;
  Event eventWriteDone;

  void completeRequest(uint64_t, bool);

  // Statistics
  uint64_t readCount;
  uint64_t readFromWriteQueue;
  uint64_t writeCount;
  uint64_t writeMerged;

 public:
  Channel(ObjectData &, DRAMController *, uint8_t, uint32_t);
  ~Channel();

  // 0 = submit, 1 = wqhit, 2 = retry
  uint8_t submit(uint64_t, bool, Event, uint64_t);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

class DRAMController : public AbstractRAM {
 private:
  Config::DRAMController *ctrl;

  // DRAM Channels
  std::vector<Channel *> channels;

  // Address decoder
  Address addressLimit;

  std::function<Address(uint64_t)> decodeAddress;

  // Stat bin
  struct StatisticBin {
    // Address range
    const uint64_t begin;
    const uint64_t end;

    // Statistics
    uint64_t lastResetAt;

    uint64_t readCount;
    uint64_t writeCount;
    uint64_t readBytes;
    uint64_t writeBytes;

    double readLatencySum;
    double writeLatencySum;

    StatisticBin(uint64_t, uint64_t);

    bool inRange(uint64_t, uint64_t);

    void getStatList(std::vector<Stat> &, std::string) noexcept;
    void getStatValues(std::vector<double> &, uint64_t) noexcept;
    void resetStatValues(uint64_t) noexcept;
  };

  std::vector<StatisticBin> statbin;

  // Request splitter
  struct Entry {
    const uint64_t id;

    uint64_t counter;
    const uint64_t chunks;

    Event eid;
    uint64_t data;

    StatisticBin *stat;

    Entry(uint64_t i, uint64_t c, Event e, uint64_t d, StatisticBin *s)
        : id(i), counter(0), chunks(c), eid(e), data(d), stat(s) {}
  };

  std::unordered_map<uint64_t, Entry> entries;

  struct SubEntry {
    const uint64_t id;

    Entry *parent;

    uint64_t address;
    uint64_t submitted;

    SubEntry(uint64_t i, Entry *p, uint64_t a) : id(i), parent(p), address(a) {}
  };

  std::unordered_map<uint64_t, SubEntry> subentries;

  std::list<SubEntry *> readRetryQueue;
  std::list<SubEntry *> writeRetryQueue;
  std::list<uint64_t> readCompletionQueue;
  std::list<uint64_t> writeCompletionQueue;

  uint64_t entrySize;
  uint64_t internalEntryID;
  uint64_t internalSubentryID;

  Event eventReadRetry;
  Event eventWriteRetry;
  Event eventReadComplete;
  Event eventWriteComplete;

  void submitRequest(uint64_t, uint32_t, bool, Event, uint64_t);
  void completeRequest(uint64_t, uint64_t, bool);
  void readRetry();
  void writeRetry();

 public:
  DRAMController(ObjectData &);
  ~DRAMController();

  // For channel
  std::function<Address(uint64_t)> &getDecodeFunction();
  void triggerWriteRetry();

  void read(uint64_t address, uint32_t length, Event eid,
            uint64_t data = 0) override;
  void write(uint64_t address, uint32_t length, Event eid,
             uint64_t data = 0) override;
  uint64_t allocate(uint64_t, std::string &&, bool = false) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::DRAM

#endif
