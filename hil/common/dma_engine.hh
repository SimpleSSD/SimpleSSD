// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_COMMON_DMA_ENGINE_HH__
#define __SIMPLESSD_HIL_COMMON_DMA_ENGINE_HH__

#include <deque>
#include <unordered_map>
#include <unordered_set>

#include "sim/interface.hh"
#include "sim/object.hh"

namespace SimpleSSD::HIL {

class DMAEngine;

struct PhysicalRegion {
  uint64_t address;
  uint32_t size;
  bool ignore;

  PhysicalRegion();
  PhysicalRegion(uint64_t, uint32_t);
  PhysicalRegion(uint64_t, uint32_t, bool);
};

class DMAData {
 private:
  friend DMAEngine;

  std::vector<PhysicalRegion> prList;

  Event eid;
  uint64_t data;
  int32_t counter;

  uint64_t handledSize;
  uint64_t requestedSize;
  uint64_t bufferSize;
  uint8_t *buffer;

  void allocateBuffer(uint64_t);
  void deallocateBuffer();

 public:
  DMAData();
  DMAData(const DMAData &) = delete;
  DMAData(DMAData &&) noexcept = delete;
  ~DMAData();

  DMAData &operator=(const DMAData &&) = delete;
  DMAData &operator=(DMAData &&) = delete;
};

using DMATag = DMAData *;

const DMATag InvalidDMATag = nullptr;

union SGLDescriptor;

/**
 * \brief DMA Engine class
 *
 * DMA engine for various SSD controllers.
 */
class DMAEngine : public Object {
 private:
  DMAInterface *interface;
  Event eventDMADone;

  Event eventPRDTInitDone;
  Event eventPRPReadDone;
  Event eventSGLReadDone;

  std::unordered_set<DMATag> tagList;
  std::deque<DMATag> pendingTagList;
  std::unordered_map<DMATag, DMATag> oldTagList;

  uint64_t pageSize;

  void dmaDone();

  DMATag createTag();
  void destroyTag(DMATag);

  // PRDT related
  void prdt_readDone();

  // PRP related
  uint32_t getPRPSize(uint64_t);
  void getPRPListFromPRP(DMATag, uint64_t);
  void getPRPListFromPRP_readDone();

  // SGL related
  void parseSGLDescriptor(DMATag, SGLDescriptor *);
  void parseSGLSegment(DMATag, uint64_t, uint32_t);
  void parseSGLSegment_readDone();

 public:
  DMAEngine(ObjectData &, DMAInterface *);
  DMAEngine(const DMAEngine &) = delete;
  DMAEngine(DMAEngine &&) noexcept = default;
  virtual ~DMAEngine();

  DMAEngine &operator=(const DMAEngine &) = delete;
  DMAEngine &operator=(DMAEngine &&) = default;

  void updatePageSize(uint64_t);

  /**
   * \brief Initialize PRDT based DMA session
   *
   * Create DMATag from provided PRDT base address and PRDT length size.
   * When DMA session initialization finished, Event eid will be called.
   */
  DMATag initFromPRDT(uint64_t base, uint32_t size, Event eid,
                      uint64_t data = 0) noexcept;

  /**
   * \brief Initialize PRP based DMA session
   *
   * Create DMATag from provided two PRP addresses and buffer length size.
   * When DMA session initialization finished, Event eid will be called.
   */
  DMATag initFromPRP(uint64_t prp1, uint64_t prp2, uint32_t size, Event eid,
                     uint64_t data = 0) noexcept;

  /**
   * \brief Initialize SGL based DMA session
   *
   * Create DMATag from provided SGL segment addresses and buffer length size.
   * When DMA session initialization finished, Event eid will be called.
   */
  DMATag initFromSGL(uint64_t dptr1, uint64_t dptr2, uint32_t size, Event eid,
                     uint64_t data = 0) noexcept;

  /**
   * \brief Initialize DMA session from raw pointer
   *
   * Create DMATag from provided pointer and size.
   */
  DMATag initRaw(uint64_t base, uint32_t size) noexcept;

  /**
   * \brief Close DMA session
   *
   * Free resources about DMA session.
   */
  void deinit(DMATag tag) noexcept;

  //! DMA read with provided session
  void read(DMATag tag, uint64_t offset, uint32_t size, uint8_t *buffer,
            Event eid, uint64_t data = 0) noexcept;

  //! DMA write with provided session
  void write(DMATag tag, uint64_t offset, uint32_t size, uint8_t *buffer,
             Event eid, uint64_t data = 0) noexcept;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;

  DMATag restoreDMATag(DMATag) noexcept;
  void clearOldDMATagList() noexcept;
};

}  // namespace SimpleSSD::HIL

#endif
