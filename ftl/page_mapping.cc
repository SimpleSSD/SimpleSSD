/*
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
 */

#include "ftl/page_mapping.hh"

#include <algorithm>
#include <limits>

#include "ftl/old/ftl.hh"
#include "ftl/old/ftl_defs.hh"
#include "log/trace.hh"
#include "util/algorithm.hh"

namespace SimpleSSD {

namespace FTL {

PageMapping::PageMapping(Parameter *p, PAL::PAL *l, ConfigReader *c)
    : AbstractFTL(p, l),
      pPAL(l),
      conf(c->ftlConfig),
      pFTLParam(p),
      bReclaimMore(false) {
  for (uint32_t i = 0; i < pFTLParam->totalPhysicalBlocks; i++) {
    freeBlocks.insert(
        {i, Block(pFTLParam->pagesInBlock, pFTLParam->ioUnitInPage)});
  }

  lastFreeBlock = getFreeBlock();
}

PageMapping::~PageMapping() {}

bool PageMapping::initialize() {
  uint64_t nPagesToWarmup;
  uint64_t nTotalPages;
  uint64_t tick = 0;
  Request req(pFTLParam->ioUnitInPage);

  nTotalPages = pFTLParam->totalLogicalBlocks * pFTLParam->pagesInBlock;
  nPagesToWarmup = nTotalPages * conf.readFloat(FTL_WARM_UP_RATIO);
  nPagesToWarmup = MIN(nPagesToWarmup, nTotalPages);
  req.ioFlag.set();

  for (uint64_t lpn = 0; lpn < pFTLParam->totalLogicalBlocks;
       lpn += nTotalPages) {
    for (uint32_t page = 0; page < nPagesToWarmup; page++) {
      req.lpn = lpn + page;

      writeInternal(req, tick, false);
    }
  }

  return true;
}

void PageMapping::read(Request &req, uint64_t &tick) {
  uint64_t begin = tick;

  readInternal(req, tick);

  Logger::debugprint(Logger::LOG_FTL_PAGE_MAPPING,
                     "READ  | LPN %" PRIu64 " | %" PRIu64 " - %" PRIu64
                     " (%" PRIu64 ")",
                     req.lpn, begin, tick, tick - begin);
}

void PageMapping::write(Request &req, uint64_t &tick) {
  uint64_t begin = tick;

  writeInternal(req, tick);

  Logger::debugprint(Logger::LOG_FTL_PAGE_MAPPING,
                     "WRITE | LPN %" PRIu64 " | %" PRIu64 " - %" PRIu64
                     " (%" PRIu64 ")",
                     req.lpn, begin, tick, tick - begin);
}

void PageMapping::trim(Request &req, uint64_t &tick) {
  uint64_t begin = tick;

  trimInternal(req, tick);

  Logger::debugprint(Logger::LOG_FTL_PAGE_MAPPING,
                     "TRIM  | LPN %" PRIu64 " | %" PRIu64 " - %" PRIu64
                     " (%" PRIu64 ")",
                     req.lpn, begin, tick, tick - begin);
}

void PageMapping::format(LPNRange &range, uint64_t &tick) {
  PAL::Request req(pFTLParam->ioUnitInPage);
  std::vector<uint32_t> list;

  req.ioFlag.set();

  for (auto iter = table.begin(); iter != table.end();) {
    if (iter->first >= range.slpn && iter->first < range.slpn + range.nlp) {
      // Do trim
      auto block = blocks.find(iter->second.first);

      if (block == blocks.end()) {
        Logger::panic("Block is not in use");
      }

      block->second.invalidate(iter->second.second);

      // Collect block indices
      list.push_back(iter->second.first);

      iter = table.erase(iter);
    }
    else {
      iter++;
    }
  }

  // Get blocks to erase
  std::sort(list.begin(), list.end());
  auto last = std::unique(list.begin(), list.end());
  list.erase(last, list.end());

  // Do GC only in specified blocks
  tick = doGarbageCollection(list, tick);
}

float PageMapping::freeBlockRatio() {
  return (float)freeBlocks.size() / pFTLParam->totalPhysicalBlocks;
}

uint32_t PageMapping::getFreeBlock() {
  uint32_t blockIndex = 0;

  if (freeBlocks.size() > 0) {
    uint32_t eraseCount = std::numeric_limits<uint32_t>::max();
    auto found = freeBlocks.end();

    // Found least erased block
    for (auto iter = freeBlocks.begin(); iter != freeBlocks.end(); iter++) {
      uint32_t current = iter->second.getEraseCount();
      if (current < eraseCount) {
        eraseCount = current;
        blockIndex = iter->first;
        found = iter;
      }
    }

    // Insert found block to block list
    if (blocks.find(blockIndex) != blocks.end()) {
      Logger::panic("Corrupted");
    }

    blocks.insert({blockIndex, found->second});

    // Remove found block from free block list
    freeBlocks.erase(found);
  }
  else {
    Logger::panic("No free block left");
  }

  return blockIndex;
}

uint32_t PageMapping::getLastFreeBlock() {
  auto freeBlock = blocks.find(lastFreeBlock);

  // Sanity check
  if (freeBlock == blocks.end()) {
    Logger::panic("Corrupted");
  }

  // If current free block is full, get next block
  if (freeBlock->second.getNextWritePageIndex() == pFTLParam->pagesInBlock) {
    lastFreeBlock = getFreeBlock();

    bReclaimMore = true;
  }

  return lastFreeBlock;
}

void PageMapping::selectVictimBlock(std::vector<uint32_t> &list,
                                    uint64_t tick) {
  static const GC_MODE mode = (GC_MODE)conf.readInt(FTL_GC_MODE);
  static const EVICT_POLICY policy =
      (EVICT_POLICY)conf.readInt(FTL_GC_EVICT_POLICY);
  static uint64_t nBlocks = conf.readInt(FTL_GC_RECLAIM_BLOCK);
  std::vector<std::pair<uint32_t, float>> weight;
  uint64_t i = 0;

  list.clear();

  // Calculate number of blocks to reclaim
  if (mode == GC_MODE_0) {
    // DO NOTHING
  }
  else if (mode == GC_MODE_1) {
    static const float t = conf.readFloat(FTL_GC_RECLAIM_THRESHOLD);

    nBlocks = pFTLParam->totalPhysicalBlocks * t - freeBlocks.size();
  }
  else {
    Logger::panic("Invalid GC mode");
  }

  // reclaim one more if last free block fully used
  if (bReclaimMore) {
    bReclaimMore = false;

    nBlocks++;
  }

  // Calculate weights of all blocks
  weight.resize(blocks.size());

  if (policy == POLICY_GREEDY) {
    for (auto &iter : blocks) {
      weight.at(i).first = iter.first;
      weight.at(i).second = iter.second.getValidPageCount();

      i++;
    }
  }
  else if (policy == POLICY_COST_BENEFIT) {
    float temp;

    for (auto &iter : blocks) {
      temp = (float)iter.second.getValidPageCount() / pFTLParam->pagesInBlock;

      weight.at(i).first = iter.first;
      weight.at(i).second =
          temp / ((1 - temp) * (tick - iter.second.getLastAccessedTime()));

      i++;
    }
  }
  else {
    Logger::panic("Invalid evict policy");
  }

  // Sort weights
  std::sort(
      weight.begin(), weight.end(),
      [](std::pair<uint32_t, float> a, std::pair<uint32_t, float> b) -> bool {
        return a.second < b.second;
      });

  // Select victims
  i = 0;
  list.resize(nBlocks);

  for (auto &iter : list) {
    iter = weight.at(i++).first;
  }
}

uint64_t PageMapping::doGarbageCollection(
    std::vector<uint32_t> &blocksToReclaim, uint64_t tick) {
  PAL::Request req(pFTLParam->ioUnitInPage);
  uint64_t beginAt;
  uint64_t finishedAt = tick;

  if (blocksToReclaim.size() == 0) {
    return tick;
  }

  // For all blocks to reclaim
  for (auto &iter : blocksToReclaim) {
    auto block = blocks.find(iter);
    uint64_t lpn;

    beginAt = tick;

    if (block == blocks.end()) {
      Logger::panic("Invalid block");
    }

    // Copy valid pages to free block
    for (uint32_t pageIndex = 0; pageIndex < pFTLParam->pagesInBlock;
         pageIndex++) {
      // Valid?
      if (block->second.getPageInfo(pageIndex, lpn, req.ioFlag)) {
        // Retrive free block
        auto freeBlock = blocks.find(getLastFreeBlock());

        // Get mapping table entry
        auto mapping = table.find(lpn);

        if (mapping == table.end()) {
          Logger::panic("Invalid mapping table entry");
        }

        // Issue Read
        req.blockIndex = mapping->second.first;
        req.pageIndex = mapping->second.second;

        pPAL->read(req, beginAt);

        // Invalidate
        block->second.invalidate(pageIndex);

        // Update mapping table
        mapping->second.first = freeBlock->first;
        mapping->second.second = freeBlock->second.getNextWritePageIndex();

        freeBlock->second.write(mapping->second.second, lpn, req.ioFlag,
                                beginAt);

        // Issue Write
        req.blockIndex = mapping->second.first;
        req.pageIndex = mapping->second.second;

        pPAL->write(req, beginAt);
      }
    }

    // Erase block
    req.blockIndex = block->first;
    req.pageIndex = 0;

    eraseInternal(req, beginAt);

    // Merge timing
    finishedAt = MAX(finishedAt, beginAt);
  }

  return finishedAt;
}

void PageMapping::readInternal(Request &req, uint64_t &tick) {
  PAL::Request palRequest(req);

  auto mapping = table.find(req.lpn);

  if (mapping != table.end()) {
    palRequest.blockIndex = mapping->second.first;
    palRequest.pageIndex = mapping->second.second;

    auto block = blocks.find(palRequest.blockIndex);

    if (block == blocks.end()) {
      Logger::panic("Block is not in use");
    }

    block->second.read(palRequest.pageIndex, palRequest.ioFlag, tick);

    pPAL->read(palRequest, tick);
  }
}

void PageMapping::writeInternal(Request &req, uint64_t &tick, bool sendToPAL) {
  PAL::Request palRequest(req);
  auto mapping = table.find(req.lpn);

  if (mapping != table.end()) {
    // Check I/O map
    uint32_t max;

    auto block = blocks.find(mapping->second.first);

    if (block == blocks.end()) {
      Logger::panic("No such block");
    }

    max = block->second.getNextWritePageIndex(palRequest.ioFlag);

    if (max <= mapping->second.second) {
      // We can write data to same page
      block->second.write(mapping->second.second, req.lpn, req.ioFlag, tick);

      if (sendToPAL) {
        palRequest.blockIndex = mapping->second.first;
        palRequest.pageIndex = mapping->second.second;

        pPAL->write(palRequest, tick);
      }

      // GC if needed
      if (freeBlockRatio() < conf.readFloat(FTL_GC_THRESHOLD_RATIO)) {
        std::vector<uint32_t> list;

        selectVictimBlock(list, tick);
        doGarbageCollection(list, tick);
      }

      return;
    }
    else {
      // Invalidate current page
      block->second.invalidate(mapping->second.second);
    }
  }
  else {
    // Create empty mapping
    table.insert({req.lpn, std::pair<uint32_t, uint32_t>()});

    mapping = table.find(req.lpn);
  }

  // Write data to free block
  auto block = blocks.find(getLastFreeBlock());

  if (block == blocks.end()) {
    Logger::panic("No such block");
  }

  uint32_t pageIndex = block->second.getNextWritePageIndex();

  block->second.write(pageIndex, req.lpn, req.ioFlag, tick);

  // update mapping to table
  mapping->second.first = block->first;
  mapping->second.second = pageIndex;

  if (sendToPAL) {
    palRequest.blockIndex = mapping->second.first;
    palRequest.pageIndex = mapping->second.second;

    pPAL->write(palRequest, tick);
  }

  // GC if needed
  if (freeBlockRatio() < conf.readFloat(FTL_GC_THRESHOLD_RATIO)) {
    std::vector<uint32_t> list;

    selectVictimBlock(list, tick);
    doGarbageCollection(list, tick);
  }
}

void PageMapping::trimInternal(Request &req, uint64_t &tick) {
  auto mapping = table.find(req.lpn);

  if (mapping != table.end()) {
    auto block = blocks.find(mapping->second.first);

    if (block == blocks.end()) {
      Logger::panic("Block is not in use");
    }

    // Invalidate block
    block->second.invalidate(mapping->second.second);

    // Remove mapping
    table.erase(mapping);
  }
}

void PageMapping::eraseInternal(PAL::Request &req, uint64_t &tick) {
  static uint64_t threshold = conf.readUint(FTL_BAD_BLOCK_THRESHOLD);
  auto block = blocks.find(req.blockIndex);

  // Sanity checks
  if (block == blocks.end()) {
    Logger::panic("No such block");
  }

  if (freeBlocks.find(req.blockIndex) != freeBlocks.end()) {
    Logger::panic("Corrupted");
  }

  if (block->second.getValidPageCount() != 0) {
    Logger::panic("There are valid pages in victim block");
  }

  // Erase block
  block->second.erase();

  pPAL->erase(req, tick);

  // Check erase count
  if (block->second.getEraseCount() < threshold) {
    // Insert block to free block list
    freeBlocks.insert({req.blockIndex, block->second});
  }

  // Remove block from block list
  blocks.erase(block);
}

}  // namespace FTL

}  // namespace SimpleSSD
