/*
 * Copyright (C) 2017 CAMELab
 *
 * This file is part of SimpleSSD.
 *
 * SimpleSSD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SimpleSSD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SimpleSSD.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "util/def.hh"

namespace SimpleSSD {

LPNRange::_LPNRange() : slpn(0), nlp(0) {}

LPNRange::_LPNRange(uint64_t s, uint64_t n) : slpn(s), nlp(n) {}

namespace ICL {

Request::_Request() : reqID(0), offset(0), length(0) {}

}  // namespace ICL

namespace FTL {

Request::_Request() : reqID(0), reqSubID(0), lpn(0), offset(0), length(0) {}

}  // namespace FTL

namespace PAL {

Request::_Request()
    : reqID(0),
      reqSubID(0),
      blockIndex(0),
      pageIndex(0),
      offset(0),
      length(0) {}

Request::_Request(FTL::Request &r)
    : reqID(r.reqID),
      reqSubID(r.reqSubID),
      blockIndex(0),
      pageIndex(0),
      offset(r.offset),
      length(r.length) {}

}  // namespace PAL

}  // namespace SimpleSSD
