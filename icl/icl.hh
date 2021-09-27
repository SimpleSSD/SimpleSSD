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

namespace SimpleSSD {

namespace HIL {

class HIL;

}

namespace ICL {

namespace Manager {

class AbstractManager;

}

namespace Cache {

class AbstractCache;

}

/**
 * \brief ICL (Internal Cache Layer) class
 *
 * Defines abstract layer to the internal data buffer interface.
 */
class ICL : public Object {
 private:
  HIL::HIL *pHIL;

  FTL::FTL *pFTL;
  Manager::AbstractManager *pManager;
  Cache::AbstractCache *pCache;

  uint64_t totalLogicalPages;
  uint32_t logicalPageSize;

  std::deque<std::pair<LPN, uint32_t>> prefetchQueue;

  Event eventPrefetch;

 public:
  ICL(ObjectData &, HIL::HIL *);
  ~ICL();

  //! Set callback
  void setCallbackFunction(Event);

  //! Read/Compare request
  void read(HIL::SubRequest *);

  //! Write request
  void write(HIL::SubRequest *);

  //! Flush request
  void flush(HIL::SubRequest *);

  //! Format request
  void format(HIL::SubRequest *);

  //! Mark completion of DMA operation
  void done(HIL::SubRequest *);

  /**
   * \brief Make read-ahead/prefetch request
   *
   * This function can generate arbitrary request without making completion to
   * host.
   */
  void makeRequest(LPN, uint32_t);

  /**
   * \brief Get logical pages contains data
   *
   * To implement per-namespace bases utilization, this function requires offset
   * and length.
   */
  uint64_t getPageUsage(LPN, uint64_t);

  //! Get total logical pages in current HIL object
  uint64_t getTotalPages();

  //! Get bytesize of one logical page.
  uint32_t getLPNSize();

  /* To HIL */

  HIL::SubRequest *getSubRequest(uint64_t);
  void getQueueStatus(uint64_t &, uint64_t &) noexcept;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;

  HIL::SubRequest *restoreSubRequest(uint64_t) noexcept;
};

}  // namespace ICL

}  // namespace SimpleSSD

#endif
