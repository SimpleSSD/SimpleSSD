// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NVME_DMA_ENGINE_HH__
#define __SIMPLESSD_HIL_NVME_DMA_ENGINE_HH__

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
    uint64_t bufferSize;
    uint8_t *buffer;

    PRPInitContext();
  };

  bool inited;

  std::vector<PRP> prpList;
  uint64_t totalSize;
  uint64_t pageSize;

  Event readPRPList;
  PRPInitContext prpContext;

  uint64_t getSizeFromPRP(uint64_t);
  void getPRPListFromPRP(uint64_t, Event);
  void getPRPListFromPRP_readDone(uint64_t);

 public:
  PRPEngine(ObjectData &, DMAInterface *, uint64_t);
  ~PRPEngine();

  void init(uint64_t, uint64_t, uint64_t, Event) override;
  void initQueue(uint64_t, uint64_t, bool, Event);

  void read(uint64_t, uint64_t, uint8_t *, Event) override;
  void write(uint64_t, uint64_t, uint8_t *, Event) override;

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
    uint64_t bufferSize;
    uint8_t *buffer;

    SGLInitContext();
  };

  bool inited;

  std::vector<Chunk> chunkList;
  uint64_t totalSize;

  Event readSGL;
  SGLInitContext sglContext;

  void parseSGLDescriptor(SGLDescriptor &);
  void parseSGLSegment(uint64_t, uint32_t, Event);
  void parseSGLSegment_readDone(uint64_t);

 public:
  SGLEngine(ObjectData &, DMAInterface *);
  ~SGLEngine();

  void init(uint64_t, uint64_t, uint64_t, Event) override;

  void read(uint64_t, uint64_t, uint8_t *, Event) override;
  void write(uint64_t, uint64_t, uint8_t *, Event) override;

  void createCheckpoint(std::ostream &) noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
