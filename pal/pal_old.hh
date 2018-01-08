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

#ifndef __PAL_PAL_OLD__
#define __PAL_PAL_OLD__

#include <cinttypes>
#include <vector>

#include "pal/abstract_pal.hh"
#include "util/old/SimpleSSD_types.h"

class PAL2;
class PALStatistics;
class Latency;

namespace SimpleSSD {

namespace PAL {

class PALOLD : public AbstractPAL {
 private:
  ::PAL2 *pal;
  ::PALStatistics *stats;
  ::Latency *lat;

  void convertCPDPBP(Request &, std::vector<::CPDPBP> &);
  void printCPDPBP(::CPDPBP &, const char *);
  void printPPN(Request &, const char *);

 public:
  PALOLD(Parameter &, Config &);
  ~PALOLD();

  void read(Request &, uint64_t &) override;
  void write(Request &, uint64_t &) override;
  void erase(Request &, uint64_t &) override;
};

}  // namespace PAL

}  // namespace SimpleSSD

#endif
