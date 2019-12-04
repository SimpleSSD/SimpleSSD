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

namespace SimpleSSD::ICL {

class AbstractCache;

/**
 * \brief ICL (Internal Cache Layer) class
 *
 * Defines abstract layer to the internal data buffer interface.
 */
class ICL : public Object {
 private:
  FTL::FTL *pFTL;
  AbstractCache *pCache;

  bool enabled;
  uint64_t totalLogicalPages;
  uint32_t logicalPageSize;

 public:
  ICL(ObjectData &);
  ~ICL();

  //! Submit request
  void submit(SubRequest *);

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
  uint32_t getLPNSize();

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::ICL

#endif
