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

  PhysicalRegion() : address(0), size(0), ignore(true) {}
  PhysicalRegion(uint64_t a, uint32_t s) : address(a), size(s), ignore(false) {}
  PhysicalRegion(uint64_t a, uint32_t s, bool i)
      : address(a), size(s), ignore(i) {}
};

class DMAData {
 private:
  friend DMAEngine;

  std::vector<PhysicalRegion> prList;

  Event eid;
  int32_t counter;

 public:
  DMAData() : eid(InvalidEventID), counter(0) {}
  DMAData(const DMAData &) = delete;
  DMAData(DMAData &&) noexcept = delete;

  DMAData &operator=(const DMAData &&) = delete;
  DMAData &operator=(DMAData &&) = delete;
};

using DMATag = DMAData *;

/**
 * \brief DMA Engine abstract class
 *
 * DMA engine class for SSD controllers.
 */
class DMAEngine : public Object {
 protected:
  DMAInterface *interface;
  Event eventDMADone;

  std::unordered_set<DMATag> tagList;
  std::deque<DMATag> pendingTagList;
  std::unordered_map<DMATag, DMATag> oldTagList;

  void dmaDone();

  DMATag createTag();
  void destroyTag(DMATag);

 public:
  DMAEngine(ObjectData &, DMAInterface *);
  DMAEngine(const DMAEngine &) = delete;
  DMAEngine(DMAEngine &&) noexcept = default;
  virtual ~DMAEngine();

  DMAEngine &operator=(const DMAEngine &) = delete;
  DMAEngine &operator=(DMAEngine &&) = default;

  /**
   * \brief Initialize DMA session
   *
   * Create DMATag from provided DMA info structure base and buffer length size.
   * When DMA session initialization finished, Event eid will be called.
   */
  virtual DMATag init(uint64_t base, uint32_t size, Event eid) noexcept = 0;

  /**
   * \brief Initialize DMA session
   *
   * Same as above, but uses 128bit base address.
   */
  virtual DMATag init(uint64_t base1, uint64_t base2, uint32_t size,
                      Event eid) noexcept = 0;

  /**
   * \brief Close DMA session
   *
   * Free resource about DMA session. You must call this function after all DMA
   * operations are finished, and you will not call anymore.
   */
  virtual void deinit(DMATag tag) noexcept = 0;

  //! DMA read with provided session
  virtual void read(DMATag tag, uint64_t offset, uint32_t size, uint8_t *buffer,
                    Event eid) noexcept = 0;

  //! DMA write with provided session
  virtual void write(DMATag tag, uint64_t offset, uint32_t size,
                     uint8_t *buffer, Event eid) noexcept = 0;

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
