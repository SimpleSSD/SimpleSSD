// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Gieseo Park <gieseo@camelab.org>
 *         Jie Zhang <jie@camelab.org>
 *         Donghyun Gouk <kukdh1@camelab.org>
 */

#include "LatencyTLC.h"

using namespace SimpleSSD;

LatencyTLC::LatencyTLC(ConfigReader *config) : Latency(config) {
  read[0] = timing->tDS + timing->tWB + timing->tR[0] + timing->tRR;
  write[0] = timing->tPROG[0] + timing->tWP + timing->tDH;
  read[1] = timing->tDS + timing->tWB + timing->tR[1] + timing->tRR;
  write[1] = timing->tPROG[1] + timing->tWP + timing->tDH;
  read[2] = timing->tDS + timing->tWB + timing->tR[2] + timing->tRR;
  write[2] = timing->tPROG[2] + timing->tWP + timing->tDH;
  erase = timing->tBERS;
}

LatencyTLC::~LatencyTLC() {}

void LatencyTLC::printTiming(SimpleSSD::Log *log,
                             void (*print)(SimpleSSD::Log *, const char *,
                                           ...)) {
  print(log, "TLC NAND timing:");
  print(log, "Operation |     LSB    |     CSB    |     MSB    |    DMA 0   |  "
             "  DMA 1");
  print(log,
        "   READ   | %10" PRIu64 " | %10" PRIu64 " | %10" PRIu64 " | %10" PRIu64
        " | %10" PRIu64,
        read[0], read[1], read[2], readdma0, readdma1);
  print(log,
        "   WRITE  | %10" PRIu64 " | %10" PRIu64 " | %10" PRIu64 " | %10" PRIu64
        " | %10" PRIu64,
        write[0], write[1], write[2], writedma0, writedma1);
  print(log,
        "   ERASE  |                           %10" PRIu64 " | %10" PRIu64
        " | %10" PRIu64,
        erase, erasedma0, erasedma1);
}

inline uint8_t LatencyTLC::GetPageType(uint32_t AddrPage) {
  return (AddrPage <= 5) ? (uint8_t)PAGE_LSB
                         : ((AddrPage <= 7) ? (uint8_t)PAGE_CSB
                                            : (((AddrPage - 8) >> 1) % 3));
}

uint64_t LatencyTLC::GetLatency(uint32_t AddrPage, uint8_t Oper, uint8_t Busy) {
  uint8_t pType = GetPageType(AddrPage);

  switch (Busy) {
    case BUSY_DMA0:
      if (Oper == OPER_READ) {
        return readdma0;
      }
      else if (Oper == OPER_WRITE) {
        return writedma0;
      }
      else {
        return erasedma0;
      }

      break;
    case BUSY_DMA1:
      if (Oper == OPER_READ) {
        return readdma1;
      }
      else if (Oper == OPER_WRITE) {
        return writedma1;
      }
      else {
        return erasedma1;
      }

      break;
    case BUSY_MEM: {
      if (Oper == OPER_READ) {
        return read[pType];
      }
      else if (Oper == OPER_WRITE) {
        return write[pType];
      }
      else {
        return erase;
      }

      break;
    }
    default:
      break;
  }

  return 10;
}

void LatencyTLC::backup(std::ostream &out) const {
  Latency::backup(out);

  BACKUP_SCALAR(out, read[0]);
  BACKUP_SCALAR(out, read[1]);
  BACKUP_SCALAR(out, read[2]);
  BACKUP_SCALAR(out, write[0]);
  BACKUP_SCALAR(out, write[1]);
  BACKUP_SCALAR(out, write[2]);
  BACKUP_SCALAR(out, erase);
}

void LatencyTLC::restore(std::istream &in) {
  Latency::restore(in);

  RESTORE_SCALAR(in, read[0]);
  RESTORE_SCALAR(in, read[1]);
  RESTORE_SCALAR(in, read[2]);
  RESTORE_SCALAR(in, write[0]);
  RESTORE_SCALAR(in, write[1]);
  RESTORE_SCALAR(in, write[2]);
  RESTORE_SCALAR(in, erase);
}
