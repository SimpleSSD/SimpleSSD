// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Gieseo Park <gieseo@camelab.org>
 *         Jie Zhang <jie@camelab.org>
 *         Donghyun Gouk <kukdh1@camelab.org>
 */

#include "LatencySLC.h"

using namespace SimpleSSD;

LatencySLC::LatencySLC(ConfigReader *config) : Latency(config) {
  read = timing->tDS + timing->tWB + timing->tR[0] + timing->tRR;
  write = timing->tPROG[0] + timing->tWP + timing->tDH;
  erase = timing->tBERS;
}

LatencySLC::~LatencySLC() {}

inline uint8_t LatencySLC::GetPageType(uint32_t) {
  return PAGE_LSB;
}

uint64_t LatencySLC::GetLatency(uint32_t, uint8_t Oper, uint8_t Busy) {
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
    case BUSY_MEM:
      if (Oper == OPER_READ) {
        return read;
      }
      else if (Oper == OPER_WRITE) {
        return write;
      }
      else {
        return erase;
      }

      break;
    default:
      break;
  }

  return 10;
}

void LatencySLC::backup(std::ostream &out) const {
  Latency::backup(out);

  BACKUP_SCALAR(out, read);
  BACKUP_SCALAR(out, write);
  BACKUP_SCALAR(out, erase);
}

void LatencySLC::restore(std::istream &in) {
  Latency::restore(in);

  RESTORE_SCALAR(in, read);
  RESTORE_SCALAR(in, write);
  RESTORE_SCALAR(in, erase);
}
