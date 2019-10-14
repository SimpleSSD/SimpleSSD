// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_ICL_ICL_HH__
#define __SIMPLESSD_ICL_ICL_HH__

#include <deque>

#include "ftl/ftl.hh"
#include "sim/object.hh"

namespace SimpleSSD ::ICL {

class AbstractCache;

struct Request {
  uint64_t id;
  uint64_t sid;

  Event eid;
  uint64_t data;

  uint64_t address;
  uint32_t skipFront;
  uint32_t skipEnd;
  uint8_t *buffer;

  Request();
  Request(uint64_t, uint64_t, Event, uint64_t, uint64_t, uint32_t, uint32_t,
          uint8_t *);
};

/**
 * \brief ICL (Internal Cache Layer) class
 *
 * Defines abstract layer to the internal data buffer interface.
 *
 * Provides five basic operations - read, write, flush, trim and format.
 * TRIM and format is simillar - both operation erases user data.
 *
 * Use LPN as logical page notation (should be unsigned integral type).
 * See std::is_integral and std::is_unsigned.
 */
// ENABLE THIS AFTER DEBUGGING
using LPN = uint64_t;
// template <class LPN, std::enable_if_t<std::is_unsigned_v<LPN>, LPN> = 0>
class ICL : public Object {
 private:
  FTL::FTL *pFTL;
  AbstractCache *pCache;

  std::deque<Request> pendingQueue;

  uint64_t totalLogicalPages;
  uint32_t logicalPageSize;

  bool enabled;

 public:
  ICL(ObjectData &);
  ICL(const ICL &) = delete;
  ICL(ICL &&) noexcept = default;
  ~ICL();

  ICL &operator=(const ICL &) = delete;
  ICL &operator=(ICL &&) = default;

  //! Submit request
  void submit(Request &&);

  //! Submit request to FTL
  void submit(FTL::Request &&);

  /**
   * \brief Get logical pages contains data
   *
   * To implement per-namespace bases utilization, this function requires offset
   * and length.
   */
  LPN getPageUsage(LPN offset, LPN length);

  //! Get total logical pages in current HIL object
  LPN getTotalPages();

  //! Get bytesize of one logical page.
  uint64_t getLPNSize();

  //! Enable/disable ICL
  void setCache(bool);

  //! Get cache enabled
  bool getCache();

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::ICL

#endif
