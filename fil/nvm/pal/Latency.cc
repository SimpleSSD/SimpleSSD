// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Gieseo Park <gieseo@camelab.org>
 *         Jie Zhang <jie@camelab.org>
 *         Donghyun Gouk <kukdh1@camelab.org>
 */

#include "Latency.h"

/*==============================
    Latency
==============================*/

/*
    Latency class is inherited to:
    - TLC
    - MLC
    - SLC
*/

using namespace SimpleSSD;

Latency::Latency(ConfigReader *config)
    : timing(config->getNANDTiming()), power(config->getNANDPower()) {
  uint64_t cmdlatch = timing->tCS + timing->tDH;
  uint64_t address = cmdlatch + (timing->tDS + timing->tDH) * 5;
  auto pagesize = config->getNANDStructure()->pageSize;

  readdma0 = cmdlatch + address + cmdlatch;
  readdma1 = timing->tWP + timing->tWC * pagesize + timing->tDH;
  writedma0 =
      cmdlatch + address + timing->tADL + timing->tRC * pagesize + cmdlatch;
  writedma1 = timing->tWP + timing->tDH;
  erasedma0 = cmdlatch + address;
  erasedma1 = writedma1;

  powerbus = power->pVCC * power->current.pICC5;
  powerread = power->pVCC * power->current.pICC1;
  powerwrite = power->pVCC * power->current.pICC2;
  powererase = power->pVCC * power->current.pICC3;
  powerstandby = power->pVCC * power->current.pISB;
}

Latency::~Latency() {}

// Unit conversion: mV * uA = nW
uint64_t Latency::GetPower(uint8_t Oper, uint8_t Busy) {
  switch (Busy) {
    case BUSY_DMA0:
    case BUSY_DMA1:
      return powerbus;
    case BUSY_MEM:
      if (Oper == OPER_READ) {
        return powerread;
      }
      else if (Oper == OPER_WRITE) {
        return powerwrite;
      }
      else {
        return powererase;
      }

      break;
    default:
      return powerstandby;
  }
}

void Latency::backup(std::ostream &out) const {
  BACKUP_SCALAR(out, readdma0);
  BACKUP_SCALAR(out, readdma1);
  BACKUP_SCALAR(out, writedma0);
  BACKUP_SCALAR(out, writedma1);
  BACKUP_SCALAR(out, erasedma0);
  BACKUP_SCALAR(out, erasedma1);
  BACKUP_SCALAR(out, powerbus);
  BACKUP_SCALAR(out, powerread);
  BACKUP_SCALAR(out, powerwrite);
  BACKUP_SCALAR(out, powererase);
  BACKUP_SCALAR(out, powerstandby);
}

void Latency::restore(std::istream &in) {
  RESTORE_SCALAR(in, readdma0);
  RESTORE_SCALAR(in, readdma1);
  RESTORE_SCALAR(in, writedma0);
  RESTORE_SCALAR(in, writedma1);
  RESTORE_SCALAR(in, erasedma0);
  RESTORE_SCALAR(in, erasedma1);
  RESTORE_SCALAR(in, powerbus);
  RESTORE_SCALAR(in, powerread);
  RESTORE_SCALAR(in, powerwrite);
  RESTORE_SCALAR(in, powererase);
  RESTORE_SCALAR(in, powerstandby);
}