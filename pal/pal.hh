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

#ifndef __PAL_PAL__
#define __PAL_PAL__

#include "util/config.hh"

namespace SimpleSSD {

namespace PAL {

typedef struct {
  uint32_t channel;        //!< Total # channels
  uint32_t package;        //!< # packages / channel
  uint32_t die;            //!< # dies / package
  uint32_t plane;          //!< # planes / die
  uint32_t block;          //!< # blocks / plane
  uint32_t page;           //!< # pages / block
  uint64_t superBlock;     //!< Total super blocks
  uint64_t superPage;      //!< Total super pages
  uint32_t pageSize;       //!< Size of page in bytes
  uint32_t superPageSize;  //!< Size of super page in bytes
} Parameter;

class PAL {
 private:
  Parameter param;

  ConfigReader *pConf;

 public:
  PAL(ConfigReader *);
  ~PAL();

  void read(uint32_t, uint32_t, uint64_t &);
  void write(uint32_t, uint32_t, uint64_t &);
  void erase(uint32_t, uint64_t &);

  Parameter *getInfo();
};

}  // namespace PAL

}  // namespace SimpleSSD

#endif
