// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __HIL_NVME_DMA_ENGINE_HH__
#define __HIL_NVME_DMA_ENGINE_HH__

#include <vector>

#include "hil/common/dma_engine.hh"

namespace SimpleSSD::HIL::NVMe {

/**
 * \brief PRP Engine class
 *
 * Parse PRP and PRP lists from PRP1/2 of SQ entry. Provides read and write
 * function to host memory.
 */
class PRPEngine : public DMAEngine {
 private:
  class PRP {
   public:
    uint64_t address;
    uint64_t size;

    PRP();
    PRP(uint64_t, uint64_t);
  };

  class PRPInitContext {
   public:
    uint64_t handledSize;
    uint8_t *buffer;
    uint64_t bufferSize;
    Event eid;
    EventContext context;
  };

  bool inited;

  std::vector<PRP> prpList;
  uint64_t totalSize;
  uint64_t pageSize;

  Event readPRPList;

  uint64_t getSizeFromPRP(uint64_t);
  void getPRPListFromPRP(uint64_t, Event, EventContext);
  void getPRPListFromPRP_readDone(uint64_t, PRPInitContext *s);

 public:
  PRPEngine(ObjectData &, Interface *, uint64_t);
  ~PRPEngine();

  void initData(uint64_t, uint64_t, uint64_t, Event, EventContext);
  void initQueue(uint64_t, uint64_t, bool, Event, EventContext);

  void read(uint64_t, uint64_t, uint8_t *, Event, EventContext) override;
  void write(uint64_t, uint64_t, uint8_t *, Event, EventContext) override;

  void createCheckpoint(std::ostream &) noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

/**
 * \brief SGL Engine class
 *
 * Parse SGL descriptors and SGL segments from PRP1/2 of SQ entry. Provides
 * read and write function to host memory.
 */
class SGLEngine : public DMAEngine {
 private:
  union SGLDescriptor {
    uint8_t data[16];
    struct {
      uint64_t address;
      uint32_t length;
      uint8_t reserved[3];
      uint8_t id;
    };

    SGLDescriptor();
  };

  class Chunk {
   public:
    uint64_t address;
    uint32_t length;

    bool ignore;

    Chunk();
    Chunk(uint64_t, uint32_t, bool);
  };

  class SGLInitContext {
   public:
    uint8_t *buffer;
    uint64_t bufferSize;
    Event eid;
    EventContext context;
  };

  bool inited;

  std::vector<Chunk> chunkList;
  uint64_t totalSize;

  Event readSGL;

  void parseSGLDescriptor(SGLDescriptor &);
  void parseSGLSegment(uint64_t, uint32_t, Event, EventContext);
  void parseSGLSegment_readDone(uint64_t, SGLInitContext *);

 public:
  SGLEngine(ObjectData &, Interface *);
  ~SGLEngine();

  void init(uint64_t, uint64_t, Event, EventContext);

  void read(uint64_t, uint64_t, uint8_t *, Event, EventContext) override;
  void write(uint64_t, uint64_t, uint8_t *, Event, EventContext) override;

  void createCheckpoint(std::ostream &) noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
