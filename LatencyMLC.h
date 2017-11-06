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

#ifndef __LatencyMLC_h__
#define __LatencyMLC_h__

#include "Latency.h"

class LatencyMLC : public Latency
{
    public:
        LatencyMLC(uint32_t mhz, uint32_t pagesize);

        uint64_t GetLatency(uint32_t AddrPage, uint8_t Oper, uint8_t Busy);
        inline uint8_t  GetPageType(uint32_t AddrPage);
};

#endif //__LatencyMLC_h__
