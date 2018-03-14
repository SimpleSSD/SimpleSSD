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

#ifndef __HIL_HIL__
#define __HIL_HIL__

#include <list>

#include "icl/icl.hh"
#include "util/config.hh"
#include "util/def.hh"

namespace SimpleSSD {

namespace HIL {

class HIL : public StatObject {
 private:
  ConfigReader *pConf;
  ICL::ICL *pICL;

  uint64_t reqCount;

  struct {
    uint64_t request[2];
    uint64_t busy[3];
    uint64_t iosize[2];
    uint64_t lastBusyAt[3];
  } stat;

  void updateBusyTime(int, uint64_t, uint64_t);

 public:
  HIL(ConfigReader *);
  ~HIL();

  void read(ICL::Request &, uint64_t &);
  void write(ICL::Request &, uint64_t &);
  void flush(ICL::Request &, uint64_t &);
  void trim(ICL::Request &, uint64_t &);

  void format(LPNRange &, bool, uint64_t &);

  void getLPNInfo(uint64_t &, uint32_t &);
  uint64_t getUsedPageCount();

  void getStats(std::vector<Stats> &) override;
  void getStatValues(std::vector<uint64_t> &) override;
  void resetStats() override;
};

}  // namespace HIL

}  // namespace SimpleSSD

#endif
