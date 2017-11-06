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

Latency::Latency(uint32_t mhz, uint32_t pagesize)
{
    switch (mhz) //base = 50mhz
    {
        case 50:  SPDIV = 1; break;
        case 100: SPDIV = 2; break;
        case 200: SPDIV = 4; break;
        case 400: SPDIV = 8; break;
        case 800: SPDIV = 16; break;
        case 1600: SPDIV = 32; break;
        default: printf("** unsupported DMA Speed\n"); std::terminate(); break;
    }
    switch (pagesize) //base = 8KB
    {
        default: printf("** unsupported page size\n"); std::terminate(); break;
		case 16384: PGDIV = 1; break;
        case 8192: PGDIV = 2; break;
        case 4096: PGDIV = 4; break;
        case 2048: PGDIV = 8; break;
        case 1024: PGDIV = 16; break;
    }
};
