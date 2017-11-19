/**
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
 *
 * Authors: Donghyun Gouk <kukdh1@camelab.org>
 */

#ifndef __SIMPLESSD_FTL_COMMAND_HH__
#define __SIMPLESSD_FTL_COMMAND_HH__

#include <cinttypes>
#include <list>

#include "util/old/SimpleSSD_types.h"

typedef struct _Command {
  Tick arrived;
  Tick finished;
  Addr ppn;
  PAL_OPERATION operation;
  bool mergeSnapshot;
  uint64_t size;

  _Command()
      : arrived(0),
        finished(0),
        ppn(0),
        operation(OPER_NUM),
        mergeSnapshot(false),
        size(0) {}
  _Command(Tick t, Addr a, PAL_OPERATION op, uint64_t s)
      : arrived(t),
        finished(0),
        ppn(a),
        operation(op),
        mergeSnapshot(false),
        size(s) {}

  Tick getLatency() {
    if (finished > 0) {
      return finished - arrived;
    }
    else {
      return 0;
    }
  }
} Command;

#endif
