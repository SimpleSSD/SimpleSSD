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

#include "LatencyTLC.h"

LatencyTLC::LatencyTLC(uint32_t mhz, uint32_t pagesize)
: Latency(mhz, pagesize)
{
    ;
}

inline uint8_t LatencyTLC::GetPageType(uint32_t AddrPage)
{
    return (AddrPage<=5)?(uint8_t)PAGE_LSB : ( (AddrPage<=7)?(uint8_t)PAGE_CSB : (((AddrPage-8)>>1)%3) );
}

uint64_t LatencyTLC::GetLatency(uint32_t AddrPage, uint8_t Oper, uint8_t Busy)
{
    #if 1
    //ps
  uint64_t lat_tbl[3][5] =  //uint32_t lat_tbl[3][5] = //Gieseo,is this right?
    {                /*  LSB           CSB         MSB         DMA0,                  DMA1*/
        /* Read  */{   58000000,   78000000,  107000000,    100000/SPDIV,         185000000*2/(PGDIV*SPDIV) },
        /* Write */{  558000000, 2201000000, 5001000000, 185000000*2/(PGDIV*SPDIV), 100000/SPDIV },
        /* Erase */{ 2274000000, 2274000000, 2274000000,   1500000/SPDIV,         100000/SPDIV }
    };
    #else
    //ns
    uint64_t lat_tbl[3][5] =  //uint32_t lat_tbl[3][5] = //Gieseo,is this right?
    {                /*  LSB      CSB      MSB    DMA0,  DMA1*/
        /* Read  */{   58000,   78000,  107000,    100/SPDIV, 185000/(PGDIV*SPDIV) },
        /* Write */{  558000, 2201000, 5001000, 185000/(PGDIV*SPDIV),    100/SPDIV },
        /* Erase */{ 2274000, 2274000, 2274000,   1500/SPDIV,    100/SPDIV }
    };
    #endif

    switch(Busy)
    {
        case BUSY_DMA0:
            return lat_tbl[Oper][3];
        case BUSY_DMA1:
            return lat_tbl[Oper][4];
        case BUSY_MEM:
        {
            //uint8_t ptype = (AddrPage<=5)?PAGE_LSB : ( (AddrPage<=7)?PAGE_CSB : (((AddrPage-8)>>1)%3) );
            uint8_t ptype = GetPageType(AddrPage);
            uint64_t ret= lat_tbl[Oper][ ptype ];

            #if DBG_PRINT_TICK
            //    printf("LAT %s page_%u(%s) = %llu\n", OPER_STRINFO[Oper], AddrPage, PAGE_STRINFO[ptype], ret);
            #endif
            return ret;
        }
        default:
            break;
    }
    return 10;
}
