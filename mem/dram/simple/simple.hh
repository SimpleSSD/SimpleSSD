// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_MEM_DRAM_SIMPLE_SIMPLE_HH__
#define __SIMPLESSD_MEM_DRAM_SIMPLE_SIMPLE_HH__

#include "mem/dram/abstract_dram.hh"
#include "util/scheduler.hh"

namespace SimpleSSD::Memory::DRAM {

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

class Request {
 public:
  bool read;
  Address address;

  Event eid;
  uint64_t data;

  uint64_t beginAt;

  Request() : read(true), eid(InvalidEventID), data(0), beginAt(0) {}
  Request(bool r, Event e, uint64_t d) : read(r), eid(e), data(d), beginAt(0) {}
  Request(const Request &) = delete;
  Request(Request &&) noexcept = default;

  Request &operator=(const Request &) = delete;
  Request &operator=(Request &&) = default;

  static void backup(std::ostream &out, Request *item) {
    BACKUP_SCALAR(out, item->read);
    BACKUP_SCALAR(out, item->address.data);

    BACKUP_EVENT(out, item->eid);
    BACKUP_SCALAR(out, item->data);

    BACKUP_SCALAR(out, item->beginAt);
  }

  static Request *restore(std::istream &in, ObjectData &object) {
    auto item = new Request();

    RESTORE_SCALAR(in, item->read);
    RESTORE_SCALAR(in, item->address.data);

    RESTORE_EVENT(in, item->eid);
    RESTORE_SCALAR(in, item->data);

    RESTORE_SCALAR(in, item->beginAt);

    return item;
  }
};

/**
 * \brief Simple DRAM model
 *
 * Simple DRAM model with Rank/Bank/Row/Col calculation.
 * Inspired by DRAMSim2.
 */
class SimpleDRAM : public AbstractDRAM {
 private:
  using RequestScheduler = SingleScheduler<Request *>;

  Address addressLimit;
  std::function<Address(uint64_t)> decodeAddress;

  uint64_t activateLatency;
  uint64_t ioReadLatency;
  uint64_t ioWriteLatency;
  uint64_t burstLatency;
  uint64_t burstSize;
  uint64_t burstInRow;

  uint64_t preSubmit(Request *);
  void postDone(Request *);

  bool checkRow(Request *);

  inline uint32_t getOffset(Address &addr) {
    return addr.bank + (uint32_t)addr.rank * pStructure->bank +
           (uint32_t)addr.channel * pStructure->bank * pStructure->rank;
  }

  std::vector<RequestScheduler *> scheduler;
  std::vector<uint32_t> rowOpened;

  CountStat readHit;
  CountStat writeHit;
  BusyStat readBusy;
  BusyStat writeBusy;
  BusyStat totalBusy;

 public:
  SimpleDRAM(ObjectData &);
  ~SimpleDRAM();

  void read(uint64_t, Event, uint64_t = 0) override;
  void write(uint64_t, Event, uint64_t = 0) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::Memory::DRAM

#endif
