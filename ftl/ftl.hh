// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_FTL_HH__
#define __SIMPLESSD_FTL_FTL_HH__

#include <deque>

#include "fil/fil.hh"
#include "sim/object.hh"

namespace SimpleSSD::FTL {

class AbstractFTL;

typedef struct {
  uint64_t totalPhysicalPages;
  uint64_t totalLogicalPages;
  uint32_t pageSize;
  uint32_t parallelismLevel1;  //!< 1st parallelism group (Channel when CWDP)
  uint32_t parallelismLevel2;  //!< 2nd parallelism group (Way when CWDP)
  uint32_t parallelismLevel3;  //!< 3rd parallelism group (Die when CWDP)
  uint32_t parallelismLevel4;  //!< 4th parallelism group (Plane when CWDP)
  uint8_t superpageLevel;      //!< Number of levels (1~N) included in superpage
} Parameter;

enum class Operation : uint8_t {
  Read,
  Write,
  Invalidate,
};

struct Request {
  uint64_t id;
  uint64_t sid;

  Event eid;
  uint64_t data;

  Operation opcode;

  uint64_t address;
  uint8_t *buffer;

  Request();
  Request(uint64_t, uint64_t, Event, uint64_t, Operation, uint64_t, uint8_t *);
};

/**
 * \brief FTL (Flash Translation Layer) class
 *
 * Defines abstract layer to the flash translation layer.
 */
class FTL : public Object {
 private:
  Parameter param;
  FIL::FIL *pFIL;

  AbstractFTL *pFTL;

 public:
  FTL(ObjectData &);
  FTL(const FTL &) = delete;
  FTL(FTL &&) noexcept = default;
  ~FTL();

  FTL &operator=(const FTL &) = delete;
  FTL &operator=(FTL &&) = default;

  void submit(Request &&);

  Parameter *getInfo();

  LPN getPageUsage(LPN offset, LPN length);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL

#endif
