// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __HIL_NVME_QUEUE_HH__
#define __HIL_NVME_QUEUE_HH__

#include <cinttypes>

#include "hil/nvme/def.hh"
#include "hil/nvme/dma_engine.hh"

namespace SimpleSSD::HIL::NVMe {

union SQEntry {
  uint8_t data[64];
  struct {
    struct {
      uint8_t opcode;
      uint8_t fuse;
      uint16_t commandID;
    } dword0;
    uint32_t namespaceID;
    uint32_t reserved1;
    uint32_t reserved2;
    uint64_t metadata;
    uint64_t data1;
    uint64_t data2;
    uint32_t dword10;
    uint32_t dword11;
    uint32_t dword12;
    uint32_t dword13;
    uint32_t dword14;
    uint32_t dword15;
  };

  SQEntry();
};

class SQEntryWrapper {
 public:
  SQEntry entry;

  uint16_t requestID;
  uint16_t sqID;
  uint16_t cqID;
  uint16_t sqHead;

  bool useSGL;

  SQEntryWrapper(SQEntry &, uint16_t, uint16_t, uint16_t, uint16_t);
};

union CQEntry {
  uint8_t data[16];
  struct {
    uint32_t dword0;
    uint32_t reserved;
    struct {
      uint16_t sqHead;
      uint16_t sqID;
    } dword2;
    struct {
      uint16_t commandID;
      uint16_t status;
    } dword3;
  };

  CQEntry();
};

class CQEntryWrapper {
 public:
  CQEntry entry;

  uint64_t submittedAt;

  uint16_t requestID;
  uint16_t cqID;
  // uint16_t sqID;  //!< sqID at entry.dword2

  CQEntryWrapper(SQEntryWrapper &);

  void makeStatus(bool, bool, StatusType, uint8_t);
};

class Queue : public Object{
 protected:
  uint16_t id;

  uint16_t head;
  uint16_t tail;

  uint16_t size;
  uint64_t stride;

  Interface *base;

 public:
  Queue(ObjectData &, uint16_t, uint16_t);
  ~Queue();

  uint16_t getID();
  uint16_t getItemCount();
  uint16_t getHead();
  uint16_t getTail();
  uint16_t getSize();
  void setBase(Interface *, uint64_t);
};

class CQueue : public Queue {
 protected:
  bool ien;
  bool phase;
  uint16_t iv;

 public:
  CQueue(ObjectData &, uint16_t, bool, uint16_t, uint16_t);

  void setData(CQEntry *, Event, void *);
  uint16_t incHead();
  void setHead(uint16_t);
  bool interruptEnabled();
  uint16_t getInterruptVector();
};

class SQueue : public Queue {
 protected:
  uint16_t cqID;
  uint8_t priority;

 public:
  SQueue(ObjectData &, uint16_t, uint8_t, uint16_t, uint16_t);

  uint16_t getCQID();
  void setTail(uint16_t);
  void getData(SQEntry *, Event, void *);
  uint8_t getPriority();
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
