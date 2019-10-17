// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FIL_FIL_HH__
#define __SIMPLESSD_FIL_FIL_HH__

#include "fil/def.hh"
#include "sim/object.hh"

namespace SimpleSSD::FIL {

class AbstractNVM;

enum class Operation : uint8_t {
  Read,
  Program,
  Erase,
  Copyback,
};

struct Request {
  uint64_t id;

  Event eid;
  uint64_t data;

  Operation opcode;

  uint64_t address;
  uint8_t *buffer;

  std::vector<uint8_t> spare;

  Request();
  Request(uint64_t, Event, uint64_t, Operation, uint64_t, uint8_t *);
  Request(uint64_t, Event, uint64_t, Operation, uint64_t, uint8_t *,
          std::vector<uint8_t> &);
};

/**
 * \brief FIL (Flash Interface Layer) class
 *
 * Defines abstract layer to the flash interface.
 */
class FIL : public Object {
 private:
  AbstractNVM *pFIL;

 public:
  FIL(ObjectData &);
  FIL(const FIL &) = delete;
  FIL(FIL &&) noexcept = default;
  ~FIL();

  FIL &operator=(const FIL &) = delete;
  FIL &operator=(FIL &&) = default;

  void submit(Request &&);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FIL

#endif
