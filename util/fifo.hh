// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_UTIL_FIFO__
#define __SIMPLESSD_UTIL_FIFO__

#include <list>

#include "sim/interface.hh"
#include "sim/object.hh"
#include "util/interface.hh"

namespace SimpleSSD {

struct FIFOParam {
  uint64_t rqSize;        //!< Read Queue size
  uint64_t wqSize;        //!< Write Queue size
  uint64_t transferUnit;  //!< Transfer unit used to interleave DMA
  DelayFunction latency;  //!< Downstream latency function
};

struct FIFOEntry {
  bool last;  //!< This is last entry of splitted request
  uint64_t id;

  uint64_t addr;
  uint64_t size;
  uint8_t *buffer;

  uint64_t arrivedAt;      //!< Request arrived to FIFO
  uint64_t insertBeginAt;  //!< Request is now begin to written on FIFO
  uint64_t insertEndAt;    //!< Request is fully written on FIFO

  Event eid;

  FIFOEntry();
  FIFOEntry(uint64_t, uint64_t, uint8_t *, uint64_t, Event);
};

struct ReadEntry {
  uint64_t id;
  uint64_t insertEndAt;
  uint64_t dmaEndAt;
  uint64_t latency;

  ReadEntry();
  ReadEntry(uint64_t, uint64_t, uint64_t, uint64_t);
};

class FIFO : public DMAInterface, public Object {
 protected:
  class Queue {
   public:
    uint64_t capacity;
    uint64_t usage;

    std::list<FIFOEntry> waitQueue;
    std::list<FIFOEntry> transferQueue;

    Event insertDone;
    Event beginTransfer;
    Event submitCompletion;
    Event transferDone;

    bool insertPending;
    bool transferPending;

    Queue(uint64_t);

    void backup(std::ostream &) noexcept;
    void restore(std::istream &) noexcept;
  };

  DMAInterface *upstream;
  FIFOParam param;
  uint64_t unitLatency;

  Queue readQueue;
  Queue writeQueue;

  uint64_t counter;
  std::list<ReadEntry> readCompletion;

  std::list<ReadEntry>::iterator find(uint64_t);

  uint64_t calcSize(uint64_t, bool &);

  // Write
  void insertWrite();
  void insertWriteDone();
  void transferWrite();
  void transferWriteDone();
  void transferWriteDoneNext();

  // Read
  void transferRead();
  void transferReadDone();
  void insertRead();
  void insertReadDone();
  void insertReadDoneMerge(std::list<ReadEntry>::iterator);
  void insertReadDoneNext();

 public:
  FIFO() = delete;
  FIFO(ObjectData &, DMAInterface *, FIFOParam &);
  FIFO(const FIFO &) = delete;
  FIFO(FIFO &&) noexcept = default;
  ~FIFO();

  FIFO &operator=(const FIFO &) = delete;
  FIFO &operator=(FIFO &&) noexcept = default;

  void read(uint64_t, uint64_t, uint8_t *, Event) override;
  void write(uint64_t, uint64_t, uint8_t *, Event) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD

#endif
