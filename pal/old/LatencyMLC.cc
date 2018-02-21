/**
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
 * Authors: Gieseo Park <gieseo@camelab.org>
 *          Jie Zhang <jie@camelab.org>
 */

#include "LatencyMLC.h"

LatencyMLC::LatencyMLC(SimpleSSD::PAL::Config::NANDTiming t) : Latency(t) {}

LatencyMLC::~LatencyMLC() {}

inline uint8_t LatencyMLC::GetPageType(uint32_t AddrPage) {
  return AddrPage % 2;
}

uint64_t LatencyMLC::GetLatency(uint32_t AddrPage, uint8_t Oper, uint8_t Busy) {
  SimpleSSD::PAL::Config::PAGETiming *pTiming = nullptr;

  switch (Busy) {
    case BUSY_DMA0:
      if (Oper == OPER_READ) {
        return timing.dma0.read;
      }
      else if (Oper == OPER_WRITE) {
        return timing.dma0.write;
      }
      else {
        return timing.dma0.erase;
      }

      break;
    case BUSY_DMA1:
      if (Oper == OPER_READ) {
        return timing.dma1.read;
      }
      else if (Oper == OPER_WRITE) {
        return timing.dma1.write;
      }
      else {
        return timing.dma1.erase;
      }

      break;
    case BUSY_MEM: {
      if (Oper == OPER_ERASE) {
        return timing.erase;
      }

      if (GetPageType(AddrPage) == PAGE_LSB) {
        pTiming = &timing.lsb;
      }
      else {
        pTiming = &timing.msb;
      }

      if (Oper == OPER_READ) {
        return pTiming->read;
      }
      else {
        return pTiming->write;
      }

      break;
    }
    default:
      break;
  }

  return 10;
}

// power
uint64_t LatencyMLC::GetPower(uint8_t Oper, uint8_t Busy) {
  // Unit(nW) = voltage(mV) x current(uA)
  // NAND for MEM: Micron NAND Flash Memory -  MT29F64G08EBAA
  // DDR for RD-DMA1/WR-DMA0: Micron Double Data Rate (DDR) SDRAM - MT46V64M4
  // LATCH for RD-DMA0/WR-DMA1: TexasInstrument LATCH - SN54ALVTH
  static const uint64_t power_tbl[4][3] = {
      /*  		DMA0	      	  MEM	 	DMA1	*/
      /* Read */ {2700 * 10, 3300 * 25000, 2600 * 500000},
      /* Write */ {2600 * 500000, 3300 * 25000, 2700 * 10},
      /* Erase */ {2700 * 10, 3300 * 25000, 2700 * 10},
      /* idle */ {3300 * 3000, 3300 * 10, 3300 * 3000}};

  // idle when Oper == 10
  if (Oper != 10) {
    switch (Busy) {
      case BUSY_DMA0:
        return power_tbl[Oper][0];
      case BUSY_DMA1:
        return power_tbl[Oper][2];
      case BUSY_MEM:
        return power_tbl[Oper][1];
      default:
        break;
    }
  }

  return power_tbl[3][1];
}
