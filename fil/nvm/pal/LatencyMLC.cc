// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Gieseo Park <gieseo@camelab.org>
 *         Jie Zhang <jie@camelab.org>
 *         Donghyun Gouk <kukdh1@camelab.org>
 */

#include "LatencyMLC.h"

using namespace SimpleSSD;

LatencyMLC::LatencyMLC(ConfigReader *config) : Latency(config) {
  read[0] = timing->tDS + timing->tWB + timing->tR[0] + timing->tRR;
  write[0] = timing->tPROG[0] + timing->tWP + timing->tDH;
  read[1] = timing->tDS + timing->tWB + timing->tR[1] + timing->tRR;
  write[1] = timing->tPROG[1] + timing->tWP + timing->tDH;
  erase = timing->tBERS;
}

LatencyMLC::~LatencyMLC() {}

void LatencyMLC::printTiming(SimpleSSD::Log *log) {
  print(log, "MLC NAND timing:");
  print(log, "Operation |     LSB    |     MSB    |    DMA 0   |    DMA 1");
  print(log,
        "   READ   | %10" PRIu64 " | %10" PRIu64 " | %10" PRIu64
        " | %10" PRIu64,
        read[0], read[1], readdma0, readdma1);
  print(log,
        "   WRITE  | %10" PRIu64 " | %10" PRIu64 " | %10" PRIu64
        " | %10" PRIu64,
        write[0], write[1], writedma0, writedma1);
  print(log,
        "   ERASE  |              %10" PRIu64 " | %10" PRIu64 " | %10" PRIu64,
        erase, erasedma0, erasedma1);
}

inline uint8_t LatencyMLC::GetPageType(uint32_t AddrPage) {
  return AddrPage % 2;
}

uint64_t LatencyMLC::GetLatency(uint32_t AddrPage, uint8_t Oper, uint8_t Busy) {
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
        return read[GetPageType(AddrPage)];
      }
      else if (Oper == OPER_WRITE) {
        return write[GetPageType(AddrPage)];
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

void LatencyMLC::backup(std::ostream &out) const {
  Latency::backup(out);

  BACKUP_SCALAR(out, read[0]);
  BACKUP_SCALAR(out, read[1]);
  BACKUP_SCALAR(out, write[0]);
  BACKUP_SCALAR(out, write[1]);
  BACKUP_SCALAR(out, erase);
}

void LatencyMLC::restore(std::istream &in) {
  Latency::restore(in);

  RESTORE_SCALAR(in, read[0]);
  RESTORE_SCALAR(in, read[1]);
  RESTORE_SCALAR(in, write[0]);
  RESTORE_SCALAR(in, write[1]);
  RESTORE_SCALAR(in, erase);
}
