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

enum class Operation : uint8_t {
  Read,    // Read data
  Write,   // Write data
  Flush,   // Cache flush
  Trim,    // Lazy erase
  Format,  // Immediate erase
};

struct Request {
  uint64_t id;

  Event eid;
  uint64_t data;

  Operation opcode;

  LPN address;

  union {
    struct {
      uint32_t skipFront;
      uint32_t skipEnd;
    };
    LPN length;  // Only used in Flush/Trim/Format
  };

  uint8_t *buffer;

  Request();
  Request(uint64_t, Event, uint64_t, Operation, LPN, uint32_t, uint32_t,
          uint8_t *);
  Request(uint64_t, Event, uint64_t, Operation, LPN, LPN);

  void backup(std::ostream &) const;
  void restore(ObjectData &, std::istream &);
};

/**
 * \brief ICL (Internal Cache Layer) class
 *
 * Defines abstract layer to the internal data buffer interface.
 */
class ICL : public Object {
 private:
  FTL::FTL *pFTL;
  AbstractCache *pCache;

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
