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

#ifndef __FTL_FTL_OLD__
#define __FTL_FTL_OLD__

#include <cinttypes>

#include "ftl/abstract_ftl.hh"
#include "ftl/ftl.hh"
#include "ftl/old/ftl_defs.hh"

class FTL;

namespace SimpleSSD {

namespace FTL {

class FTLOLD : public AbstractFTL {
 private:
  ::FTL *ftl;
  ::Parameter old;

  Config *pConf;

 public:
  FTLOLD(Parameter *, PAL::PAL *, ConfigReader *);
  ~FTLOLD();

  bool initialize() override;

  void read(Request &, uint64_t &) override;
  void write(Request &, uint64_t &) override;
  void trim(Request &, uint64_t &) override;

  void format(LPNRange &, uint64_t &) override;
};

}  // namespace FTL

}  // namespace SimpleSSD

#endif
