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

LatencyMLC::LatencyMLC(uint32_t mhz, uint32_t pagesize)
    : Latency(mhz, pagesize) {
  ;
}

inline uint8_t LatencyMLC::GetPageType(uint32_t AddrPage) {
  return AddrPage % 2;
}

uint64_t LatencyMLC::GetLatency(uint32_t AddrPage, uint8_t Oper, uint8_t Busy) {
#if 1
  // ps
  uint32_t lat_tbl[3][5] = {
      /*  LSB         MSB         DMA0,                     DMA1*/
      /* Read  */ {40000000, 65000000, 100000 / SPDIV,
                   185000000 * 2 / (PGDIV * SPDIV)},
      /* Write */
      {500000000, 1300000000, 185000000 * 2 / (PGDIV * SPDIV), 100000 / SPDIV},
      /* Erase */ {3500000000, 3500000000, 1500000 / SPDIV, 100000 / SPDIV}};
#else
  // ns
  uint32_t lat_tbl[3][5] = {
      /*  LSB      MSB    DMA0,  DMA1*/
      /* Read  */ {58000, 78000, 100 / SPDIV, 185000 / (PGDIV * SPDIV)},
      /* Write */ {558000, 2201000, 185000 / (PGDIV * SPDIV), 100 / SPDIV},
      /* Erase */ {2274000, 2274000, 1500 / SPDIV, 100 / SPDIV}};
#endif

  switch (Busy) {
    case BUSY_DMA0:
      return lat_tbl[Oper][2];
    case BUSY_DMA1:
      return lat_tbl[Oper][3];
    case BUSY_MEM: {
      uint64_t ret = lat_tbl[Oper][AddrPage % 2];
#if DBG_PRINT_TICK
//    printf("LAT %s page_%u(%s) = %llu\n", OPER_STRINFO[Oper], AddrPage,
//    PAGE_STRINFO[ptype], ret);
#endif
      return ret;
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
  uint64_t power_tbl[4][3] = {
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
