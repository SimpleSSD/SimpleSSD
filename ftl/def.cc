// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/def.hh"

namespace SimpleSSD::FTL {

Request::Request(uint64_t t)
    : tag(t),
      opcode(Operation::None),
      result(Response::Success),
      lpn(InvalidLPN),
      ppn(InvalidPPN),
      offset(0),
      length(0) {}

Request::Request(uint64_t t, Operation o, LPN l)
    : tag(t),
      opcode(o),
      result(Response::Success),
      lpn(l),
      ppn(InvalidPPN),
      offset(0),
      length(0) {}

Request::Request(uint64_t t, Operation o, LPN l, PPN p)
    : tag(t),
      opcode(o),
      result(Response::Success),
      lpn(l),
      ppn(p),
      offset(0),
      length(0) {}

}  // namespace SimpleSSD::FTL
