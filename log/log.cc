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

#include "log/log.hh"
#include "log/stat.hh"
#include "log/trace.hh"

#include <cstdarg>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

namespace SimpleSSD {

namespace Logger {

struct Logger {
  std::ostream &outfile;
  std::ostream &errfile;
  std::function<uint64_t()> curTick;

  Logger(std::ostream &o, std::ostream &e, std::function<uint64_t()> t)
      : outfile(o), errfile(e), curTick(t) {}
};

struct StatFunction {
  std::function<void(uint64_t)> &fct;
  uint64_t period;
  uint64_t callAt;

  StatFunction(std::function<void(uint64_t)> &f, uint64_t p)
      : fct(f), period(p), callAt(p) {}
};

struct Stat {
  std::ostream &outfile;
  std::vector<StatFunction> fctList;

  Stat(std::ostream &o) : outfile(o) {}
};

Logger *logger = nullptr;
Stat *stat = nullptr;

void panic(const char *format, ...) {
  va_list args, copied;
  std::vector<char> str;

  va_start(args, format);
  va_copy(copied, args);
  str.resize(vsnprintf(nullptr, 0, format, args) + 1);
  va_end(args);
  vsnprintf(str.data(), str.size(), format, copied);
  va_end(args);

  if (logger) {
    logger->errfile << logger->curTick() << ": panic: " << str.data()
                    << std::endl;
  }

  std::terminate();
}

void warn(const char *format, ...) {
  va_list args, copied;
  std::vector<char> str;

  va_start(args, format);
  va_copy(copied, args);
  str.resize(vsnprintf(nullptr, 0, format, args) + 1);
  va_end(args);
  vsnprintf(str.data(), str.size(), format, copied);
  va_end(args);

  if (logger) {
    logger->errfile << logger->curTick() << ": warn: " << str.data()
                    << std::endl;
  }
}

void info(const char *format, ...) {
  va_list args, copied;
  std::vector<char> str;

  va_start(args, format);
  va_copy(copied, args);
  str.resize(vsnprintf(nullptr, 0, format, args) + 1);
  va_end(args);
  vsnprintf(str.data(), str.size(), format, copied);
  va_end(args);

  if (logger) {
    logger->errfile << logger->curTick() << ": info: " << str.data()
                    << std::endl;
  }
}

const std::string logName[LOG_NUM] = {
    "global",             //!< LOG_COMMON
    "HIL",                //!< LOG_HIL
    "HIL::NVMe",          //!< LOG_HIL_NVME
    "ICL",                //!< LOG_ICL
    "ICL::GenericCache",  //!< LOG_ICL_GENERIC_CACHE
    "FTL",                //!< LOG_FTL
    "FTL::FTLOLD",        //!< LOG_FTL_OLD
    "PAL"                 //!< LOG_PAL
};

void debugprint(LOG_ID id, const char *format, ...) {
  va_list args, copied;
  std::vector<char> str;

  va_start(args, format);
  va_copy(copied, args);
  str.resize(vsnprintf(nullptr, 0, format, args) + 1);
  va_end(args);
  vsnprintf(str.data(), str.size(), format, copied);
  va_end(args);

  if (logger && id < LOG_NUM) {
    logger->outfile << logger->curTick() << ": " << logName[id] << ": "
                    << str.data() << std::endl;
  }
}

void debugprint(LOG_ID id, const uint8_t *buffer, uint64_t size) {
  if (logger && id < LOG_NUM) {
    uint32_t temp;

    temp = id;
    logger->outfile.write((char *)&temp, 4);
    logger->outfile.write((char *)&size, 8);
    logger->outfile.write((const char *)buffer, size);
  }
}

void initLogSystem(std::ostream &out, std::ostream &err,
                   std::function<uint64_t()> tickFct) {
  destroyLogSystem();

  logger = new Logger(out, err, tickFct);
}

void destroyLogSystem() {
  delete logger;
}

void initStatSystem(std::ostream &out) {
  destroyStatSystem();

  stat = new Stat(out);
}

void statHandler(uint64_t tick) {
  if (stat) {
    for (auto &item : stat->fctList) {
      if (item.callAt <= tick) {
        item.fct(tick);
        item.callAt = tick + item.period;
      }
    }
  }
}

void destroyStatSystem() {
  delete stat;
}

void registerStat(uint64_t period, std::function<void(uint64_t)> fct) {
  if (stat) {
    stat->fctList.push_back(StatFunction(fct, period));
  }
}

}  // namespace Logger

}  // namespace SimpleSSD
