/**
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
 *
 * Authors: Donghyun Gouk <kukdh1@camelab.org>
 */

#ifndef __BASE_CONFIG_HH__
#define __BASE_CONFIG_HH__

#include <cinttypes>
#include <string>
#include <cstring>

#include "SimpleSSD_types.h"
#include "ini.h"

#define MATCH_SECTION(str)  (strcmp(section, str) == 0)
#define MATCH_NAME(str)     (strcmp(name, str) == 0)
#define MATCH_VALUE(str)    (strcmp(value, str) == 0)

class BaseConfig {
  protected:
    static int defaultHandler(void *, const char *, const char *, const char *);
    static int64_t toInt(const char *);
    static double toDouble(const char *);

  public:
    BaseConfig(std::string);

    /** SSD Configuration area **/
    uint8_t NANDType;
    uint32_t NumChannel;
    uint32_t NumPackage;
    uint32_t NumDie;
    uint32_t NumPlane;
    uint32_t NumBlock;
    uint32_t NumPage;
    uint32_t SizePage;
    uint32_t DMAMHz;

    /** FTL Configuration area **/
    uint8_t FTLMapping;
    long double FTLOP;
    long double FTLGCThreshold;
    uint32_t FTLMapN;
    uint32_t FTLMapK;
    uint32_t FTLEraseCycle;
    int SuperblockDegree;
    double Warmup;
    uint32_t OriginalSizes[7];
    uint8_t AddrSeq[7];
    uint8_t AddrRemap[6];

    /** Utility functions **/
    uint64_t GetTotalSizeSSD();
    uint64_t GetTotalNumPage();
    uint64_t GetTotalNumBlock();
    uint64_t GetTotalNumDie();
    uint64_t GetTotalNumPlane();
};

#endif
