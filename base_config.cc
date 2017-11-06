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

#include "base_config.hh"

#include "base/misc.hh"

BaseConfig::BaseConfig(std::string path) {
  // Parse configuration file
  if (ini_parse(path.c_str(), defaultHandler, this) < 0) {
    fatal("config: Cannot open configuration file: %s\n", path.c_str());
  }

  // Codes from SimpleSSD
  OriginalSizes[ADDR_CHANNEL] = NumChannel;
  OriginalSizes[ADDR_PACKAGE] = NumPackage;
  OriginalSizes[ADDR_DIE]     = NumDie;
  OriginalSizes[ADDR_PLANE]   = NumPlane;
  OriginalSizes[ADDR_BLOCK]   = NumBlock;
  OriginalSizes[ADDR_PAGE]    = NumPage;
  OriginalSizes[6]            = 0; //Add remaining bits

  int superblock = SuperblockDegree;
  AddrSeq[0] = AddrRemap[0];
  AddrSeq[1] = AddrRemap[1];
  AddrSeq[2] = AddrRemap[2];
  AddrSeq[3] = AddrRemap[3];
  AddrSeq[4] = AddrRemap[4];
  AddrSeq[5] = AddrRemap[5];

  int offset = 0;
  while (superblock > 1) {
    if (superblock / OriginalSizes[AddrRemap[5 - offset]] == 0) { //superblock is not aligned
      OriginalSizes[6] = OriginalSizes[AddrRemap[5 - offset]] / superblock;
      AddrSeq[6] = offset;
      OriginalSizes[AddrRemap[5 - offset]] = superblock;
      superblock = 0;
    }
    else superblock = superblock / OriginalSizes[AddrRemap[5 - offset]];
    offset++;
  }

  //Search the page location
  for (int i = 0; i < 6; i++) {
    if (AddrRemap[i] == ADDR_PAGE) {
      if (i <= (5 - offset)) {  //revise address remap, if super block is inconsistent with input address remap info
        for (unsigned j = i; j < (5 - offset); j++ ) {
          AddrRemap[j] = AddrRemap[j + 1];
        }
        AddrRemap[5 - offset] = ADDR_PAGE;
      }
      else { //if address remap is not revised
        if (OriginalSizes[6] != 0) {
          OriginalSizes[AddrRemap[5 - AddrSeq[6]]] *= OriginalSizes[6];
          OriginalSizes[6] = 0;
        }
      }
      break;
    }
  }
  for (int i = 0; i < 6; i++) {
    AddrSeq[i] = AddrRemap[i];
  }
}

int BaseConfig::defaultHandler(void *context, const char* section, const char* name, const char *value) {
  BaseConfig *pThis = (BaseConfig *)context;

  // SSD configuration
  if (MATCH_SECTION("ssd")) {
    if (MATCH_NAME("NANDType")) {
      switch (toInt(value)) {
        case 0:   pThis->NANDType = NAND_SLC;     break;
        case 1:   pThis->NANDType = NAND_MLC;     break;
        case 2:   pThis->NANDType = NAND_TLC;     break;
        default:
          fatal("config: Unknown NANDType: %s\n", value);
          break;
      }
    }
    else if (MATCH_NAME("NumChannel")) {
      pThis->NumChannel = toInt(value);
    }
    else if (MATCH_NAME("NumPackage")) {
      pThis->NumPackage = toInt(value);
    }
    else if (MATCH_NAME("NumDie")) {
      pThis->NumDie = toInt(value);
    }
    else if (MATCH_NAME("NumPlane")) {
      pThis->NumPlane = toInt(value);
    }
    else if (MATCH_NAME("NumBlock")) {
      pThis->NumBlock = toInt(value);
    }
    else if (MATCH_NAME("NumPage")) {
      pThis->NumPage = toInt(value);
    }
    else if (MATCH_NAME("SizePage")) {
      pThis->SizePage = toInt(value);
    }
    else if (MATCH_NAME("DMAMhz")) {
      pThis->DMAMHz = toInt(value);
    }
  }
  else if (MATCH_SECTION("ftl")) {
    if (MATCH_NAME("FTLOP")) {
      pThis->FTLOP = toDouble(value);
    }
    else if (MATCH_NAME("FTLGCThreshold")) {
      pThis->FTLGCThreshold = toDouble(value);
    }
    else if (MATCH_NAME("FTLMapN")) {
      pThis->FTLMapN = toInt(value);
    }
    else if (MATCH_NAME("FTLMapK")) {
      pThis->FTLMapK = toInt(value);
    }
    else if (MATCH_NAME("FTLEraseCycle")) {
      pThis->FTLEraseCycle = toInt(value);
    }
    else if (MATCH_NAME("SuperblockDegree")) {
      pThis->SuperblockDegree = toInt(value);
    }
    else if (MATCH_NAME("Warmup")) {
      pThis->Warmup = toDouble(value);
    }
    else if (MATCH_NAME("AddrRemap_PAGE")) {
      pThis->AddrRemap[toInt(value)] = ADDR_PAGE;
    }
    else if (MATCH_NAME("AddrRemap_BLOCK")) {
      pThis->AddrRemap[toInt(value)] = ADDR_BLOCK;
    }
    else if (MATCH_NAME("AddrRemap_PLANE")) {
      pThis->AddrRemap[toInt(value)] = ADDR_PLANE;
    }
    else if (MATCH_NAME("AddrRemap_DIE")) {
      pThis->AddrRemap[toInt(value)] = ADDR_DIE;
    }
    else if (MATCH_NAME("AddrRemap_PACKAGE")) {
      pThis->AddrRemap[toInt(value)] = ADDR_PACKAGE;
    }
    else if (MATCH_NAME("AddrRemap_CHANNEL")) {
      pThis->AddrRemap[toInt(value)] = ADDR_CHANNEL;
    }
  }

  return 1;
}

int64_t BaseConfig::toInt(const char *str) {
  return atoll(str);
}

double BaseConfig::toDouble(const char *str) {
  return atof(str);
}

uint64_t BaseConfig::GetTotalSizeSSD() {
  return GetTotalNumPage() * SizePage;
}

uint64_t BaseConfig::GetTotalNumPage() {
  return GetTotalNumBlock() * NumPage;
}

uint64_t BaseConfig::GetTotalNumBlock() {
  return GetTotalNumPlane() * NumBlock;
}

uint64_t BaseConfig::GetTotalNumPlane() {
  return GetTotalNumDie() * NumPlane;
}

uint64_t BaseConfig::GetTotalNumDie() {
  return (uint64_t)NumChannel * NumPackage * NumDie;
}
