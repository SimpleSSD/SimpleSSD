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

#ifndef __LOG_TRACE__
#define __LOG_TRACE__

#include <cinttypes>

namespace SimpleSSD {

namespace Logger {

typedef enum {
  LOG_COMMON,
  LOG_HIL,
  LOG_HIL_NVME,
  LOG_ICL,
  LOG_ICL_GENERIC_CACHE,
  LOG_FTL,
  LOG_FTL_OLD,
  LOG_FTL_PAGE_MAPPING,
  LOG_PAL,
  LOG_PAL_OLD,
  LOG_NUM
} LOG_ID;

void debugprint(LOG_ID, const char *, ...);
void debugprint(LOG_ID, const uint8_t *, uint64_t);

void panic(const char *, ...);
void warn(const char *, ...);
void info(const char *, ...);

}  // namespace Logger

}  // namespace SimpleSSD

#endif
