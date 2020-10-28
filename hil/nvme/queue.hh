// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_SIMPLESSD_HIL_NVME_QUEUE_HH__
#define __SIMPLESSD_SIMPLESSD_HIL_NVME_QUEUE_HH__

#include <cinttypes>

#include "hil/common/dma_engine.hh"
#include "hil/nvme/def.hh"

namespace SimpleSSD::HIL::NVMe {

union SQEntry {
  uint8_t data[64];
  struct {
    struct {
      uint8_t opcode;
      uint8_t fuse : 2;
      uint8_t rsvd : 4;
      uint8_t psdt : 2;
      uint16_t commandID;
    } dword0;
    uint32_t namespaceID;
    uint32_t rsvd1;
    uint32_t rsvd2;
    uint64_t mptr;
    uint64_t dptr1;
    uint64_t dptr2;
    uint32_t dword10;
    uint32_t dword11;
    uint32_t dword12;
    uint32_t dword13;
    uint32_t dword14;
    uint32_t dword15;
  };

  SQEntry();
};

union CQEntry {
  uint8_t data[16];
  struct {
    union {
      uint64_t result;
      struct {
        uint32_t dword0;
        uint32_t dword1;
      };
    };
    struct {
      uint16_t sqHead;
      uint16_t sqID;
    } dword2;
    struct {
      uint16_t commandID;
      union {
        uint16_t status;
        struct {
          uint16_t phase : 1;
          uint16_t sc : 8;
          uint16_t sct : 3;
          uint16_t crd : 2;
          uint16_t more : 1;
          uint16_t dnr : 1;
        };
      };
    } dword3;
  };

  CQEntry();
};

class Queue : public Object {
 protected:
  uint16_t id;

  uint16_t head;
  uint16_t tail;

  uint32_t size;
  uint32_t stride;

  DMAEngine *dmaEngine;
  DMATag dmaTag;

 public:
  Queue(ObjectData &, DMAEngine *);
  Queue(ObjectData &, DMAEngine *, uint16_t, uint32_t, uint32_t);
  ~Queue();

  void setDMAData(uint64_t, bool, Event, uint64_t);

  uint16_t getID();
  uint16_t getItemCount();
  uint16_t getHead();
  uint16_t getTail();
  uint16_t getSize();

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  virtual void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

class CQueue : public Queue {
 protected:
  bool ien;
  bool phase;
  uint16_t iv;

 public:
  CQueue(ObjectData &, DMAEngine *);
  CQueue(ObjectData &, DMAEngine *, uint16_t, uint32_t, uint32_t, uint16_t,
         bool);

  void setData(CQEntry *, Event);
  uint16_t incHead();
  void setHead(uint16_t);
  bool interruptEnabled();
  uint16_t getInterruptVector();

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

class SQueue : public Queue {
 protected:
  uint16_t cqID;
  QueuePriority priority;

 public:
  SQueue(ObjectData &, DMAEngine *);
  SQueue(ObjectData &, DMAEngine *, uint16_t, uint32_t, uint32_t, uint16_t,
         QueuePriority);

  uint16_t getCQID();
  void setTail(uint16_t);
  void getData(SQEntry *, Event);
  QueuePriority getPriority();

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
