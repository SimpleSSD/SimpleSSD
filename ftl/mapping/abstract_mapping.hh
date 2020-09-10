// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 *         Junhyeok Jang <jhjang@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_MAPPING_ABSTRACT_MAPPING_HH__
#define __SIMPLESSD_FTL_MAPPING_ABSTRACT_MAPPING_HH__

#include "ftl/def.hh"
#include "hil/request.hh"
#include "sim/object.hh"

namespace SimpleSSD::FTL {

class AbstractFTL;

namespace BlockAllocator {

class AbstractAllocator;

}

namespace Mapping {

struct BlockMetadata {
  PPN blockID;

  uint32_t nextPageToWrite;
  uint64_t insertedAt;
  Bitset validPages;

  BlockMetadata() : blockID(InvalidPPN), nextPageToWrite(0), insertedAt(0) {}
  BlockMetadata(PPN id, uint32_t s)
      : blockID(id), nextPageToWrite(0), insertedAt(0), validPages(s) {}
};

class AbstractMapping : public Object {
 protected:
  using ReadEntryFunction = std::function<uint64_t(uint8_t *, uint64_t)>;
  using WriteEntryFunction = std::function<void(uint8_t *, uint64_t, uint64_t)>;
  using ParseEntryFunction = std::function<uint64_t(uint64_t &)>;
  using MakeEntryFunction = std::function<uint64_t(uint64_t, uint64_t)>;

  Parameter param;
  FIL::Config::NANDStructure *filparam;

  AbstractFTL *pFTL;
  BlockAllocator::AbstractAllocator *allocator;

  // Stat
  uint64_t requestedReadCount;
  uint64_t requestedWriteCount;
  uint64_t requestedInvalidateCount;
  uint64_t readLPNCount;
  uint64_t writeLPNCount;
  uint64_t invalidateLPNCount;

  /**
   * \brief FTL Table access function generator
   *
   * This function generates table access functions and returns table entry size
   * which is one of 2, 4, 6 or 8 bytes. This function is very useful to reduce
   * required memory size of simulation - rather than using 8 bytes for LPN/PPN,
   * we can use much smaller table entry size by using this function.
   *
   * Example table read routine:
   * \code{.cc}
   * auto entry = readFunc(tablePointer, tableIndex);
   * auto meta = parseMetaFunc(entry);
   * // Now meta contains metadata, entry contains entry value
   * \endcode
   *
   * Example table write routine:
   * \code{.cc}
   * auto entry = makeMetaFunc(entryValue, metaValue);
   * writeFunc(tablePointer, tableIndex, entry);
   * \endcode
   *
   * \param[in] total         Total number of addresses should be stored to
   *                          table. In general, this value is total physical
   *                          pages in SSD.
   * \param[in] shift         Total number of bits for per-entry metadata.
   *                          In general, this valud is 1 - we need one valid
   *                          bit per one entry.
   * \param[in] readFunc      Table read function. This function will return one
   *                          table entry extended to 8 bytes.
   * \param[in] writeFunc     Table write function. This function will write
   *                          one table entry.
   * \param[in] parseMetaFunc This function will split metadata and entry value.
   * \param[in] makeMetaFunc  This function will join metadata and entry value.
   * \return  Return entry size in bytes. One of 2, 4, 6 or 8.
   */
  inline uint64_t makeEntrySize(uint64_t total, uint64_t shift,
                                ReadEntryFunction &readFunc,
                                WriteEntryFunction &writeFunc,
                                ParseEntryFunction &parseMetaFunc,
                                MakeEntryFunction &makeMetaFunc) {
    uint64_t ret = 8;

    total <<= shift;

    // Memory consumption optimization
    if (total <= std::numeric_limits<uint16_t>::max()) {
      ret = 2;

      readFunc = [](uint8_t *table, uint64_t offset) {
        return *(uint16_t *)(table + (offset << 1));
      };
      writeFunc = [](uint8_t *table, uint64_t offset, uint64_t value) {
        *(uint16_t *)(table + (offset << 1)) = (uint16_t)value;
      };
    }
    else if (total <= std::numeric_limits<uint32_t>::max()) {
      ret = 4;

      readFunc = [](uint8_t *table, uint64_t offset) {
        return *(uint32_t *)(table + (offset << 2));
      };
      writeFunc = [](uint8_t *table, uint64_t offset, uint64_t value) {
        *(uint32_t *)(table + (offset << 2)) = (uint32_t)value;
      };
    }
    else if (total <= ((uint64_t)1ull << 48)) {
      uint64_t mask = (uint64_t)0x0000FFFFFFFFFFFF;
      ret = 6;

      readFunc = [mask](uint8_t *table, uint64_t offset) {
        return (*(uint64_t *)(table + (offset * 6))) & mask;
      };
      writeFunc = [](uint8_t *table, uint64_t offset, uint64_t value) {
        memcpy(table + offset * 6, &value, 6);
      };
    }
    else {
      ret = 8;

      readFunc = [](uint8_t *table, uint64_t offset) {
        return *(uint64_t *)(table + (offset << 3));
      };
      writeFunc = [](uint8_t *table, uint64_t offset, uint64_t value) {
        *(uint64_t *)(table + (offset << 3)) = value;
      };
    }

    shift = ret * 8 - shift;
    uint64_t mask = ((uint64_t)1 << shift) - 1;

    parseMetaFunc = [shift, mask](uint64_t &entry) {
      uint64_t ret = entry >> shift;

      entry &= mask;

      return ret;
    };
    makeMetaFunc = [shift, mask](uint64_t entry, uint64_t meta) {
      return (entry & mask) | (meta << shift);
    };

    return ret;
  }

  /**
   * \brief Insert FTL memory access command
   *
   * In FTL translation procedure, we need to access memory multiple times.
   * This helper function will collect required memory access for one
   * translation operation (until requestMemoryAccess function called).
   *
   * Just add this function when memory access is required.
   *
   * \param[in] read    True if this memory access is read.
   * \param[in] address Memory address to access
   * \param[in] size    Data size to access
   * \param[in] enable  Enable this memory access. If false, this command will
   *                    be ignored.
   */
  void insertMemoryAddress(bool read, uint64_t address, uint32_t size,
                           bool enable = true);

  /**
   * \brief Execute memory access commands + CPU firmware latency
   *
   * This function woll execute collected memory access commands by
   * insertMemoryAddress function. Set valid CPU::Function class if you want to
   * handle CPU firmware latency too.
   *
   * \param[in] eid   Callback event after memory/CPU access.
   * \param[in] data  Event context.
   * \param[in] fstat CPU firmware information.
   */
  void requestMemoryAccess(Event eid, uint64_t data, CPU::Function &fstat);

 private:
  struct MemoryCommand {
    uint64_t address;
    bool read;
    uint32_t size;

    MemoryCommand(bool b, uint64_t a, uint32_t s)
        : address(a), read(b), size(s) {}
  };

  struct CommandList {
    // Request information
    Event eid;
    uint64_t data;

    // Firmware information
    CPU::Function fstat;

    // Pending memory access list
    std::deque<MemoryCommand> cmdList;
  };

  std::deque<MemoryCommand> pendingMemoryAccess;
  std::unordered_map<uint64_t, CommandList> memoryCommandList;

  uint64_t memoryTag;
  inline uint64_t makeMemoryTag() { return ++memoryTag; }

  Event eventMemoryDone;
  void handleMemoryCommand(uint64_t);

 public:
  AbstractMapping(ObjectData &);
  virtual ~AbstractMapping() {}

  /* Functions for AbstractAllocator */

  /**
   * Return valid page count of specific block.
   *
   * \param[in] ppn Physical page address.
   * \param[in] np  Number of pages in mapping granularity.
   */
  virtual uint32_t getValidPages(PPN ppn, uint64_t np = 1) = 0;

  /**
   * Return age (inserted time) of specific block.
   *
   * \param[in] ppn Physical page address.
   * \param[in] np  Number of pages in mapping granularity.
   */
  virtual uint64_t getAge(PPN ppn, uint64_t np = 1) = 0;

  /* Functions for AbstractFTL */

  /**
   * \brief FTL initialization function
   *
   * FTL initialization should be handled in this function.
   * Initialization includes:
   *  Memory allocation by object.memory->allocate.
   *  FTL mapping table filling.
   *
   * Immediately call AbstractMapping::initialize() when you override this
   * function.
   * \code{.cc}
   * void YOUR_FTL_CLASS::initialize(AbstractFTL *f,
   *                                 BlockAllocator:AbstractAllocator *a) {
   *   AbstractMapping::initialize(f, a);
   *
   *   // Your initialization code here.
   * }
   * \endcode
   */
  virtual void initialize(AbstractFTL *, BlockAllocator::AbstractAllocator *);

  //! Return FTL parameter structure
  const Parameter *getInfo();

  /**
   * \brief Get valid page count
   *
   * This function counts valid mapping in mapping table in range.
   * This function requires starting LPN and ending LPN because NVMe supports
   * multiple volumes (namespaces) per one SSD.
   *
   * \param[in] slpn  Starting logical page address (includes).
   * \param[in] elpn  Ending logical page address (excludes).
   */
  virtual LPN getPageUsage(LPN slpn, LPN elpn) = 0;

  /**
   * \brief Perform FTL read translation
   *
   * This function translate logical page address to physical page address.
   * Store translated address with req->setPPN() and error with
   * req->setResponse().
   *
   * \param[in] req Pointer to FTL::Request.
   * \param[in] eid Callback event. Event context should be req->getTag().
   */
  virtual void readMapping(Request *req, Event eid) = 0;

  /**
   * \brief Perform FTL write translation
   *
   * This function allocates new page to store new data. Old mapping is
   * invalidated. Store address of allocated page with req->setPPN().
   *
   * \param[in] req Pointer to FTL::Request.
   * \param[in] eid Callback event. Event context should be req->getTag().
   */
  virtual void writeMapping(Request *req, Event eid) = 0;

  /**
   * \brief Perform FTL invalidation
   *
   * This function erases/invalidates mapping.
   *
   * \param[in] req Pointer to FTL::Request.
   * \param[in] eid Callback event. Event context should be req->getTag().
   */
  virtual void invalidateMapping(Request *req, Event eid) = 0;

  /**
   * \brief Get minimum and preferred mapping granularity
   *
   * Return minimum (which not making read-modify-write operation) mapping
   * granularity and preferred (which can make best performance) mapping size.
   *
   * \param[out]  min     Minimum mapping granularity
   * \param[out]  prefer  Preferred mapping granularity
   */
  virtual void getMappingSize(uint64_t *min, uint64_t *prefer = nullptr) = 0;

  /**
   * \brief Retrive page copy list
   *
   * fatal: Operation is not defined yet.
   */
  virtual void getCopyContext(CopyContext &, Event) = 0;

  /**
   * \brief Free page copy list
   *
   * fatal: Operation is not defined yet.
   */
  virtual void markBlockErased(PPN) = 0;

  //! Convert physical page address to physical block number
  inline PPN getBlockFromPPN(PPN ppn) {
    return ppn % param.totalPhysicalBlocks;
  }

  //! Convert physical page address to page index
  inline PPN getPageFromPPN(PPN ppn) { return ppn / param.totalPhysicalBlocks; }

  //! Makge physical page address from physical block number and page index
  inline PPN makePPN(PPN block, PPN page) {
    return block + page * param.totalPhysicalBlocks;
  }

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace Mapping

}  // namespace SimpleSSD::FTL

#endif
