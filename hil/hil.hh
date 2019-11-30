// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_HIL_HH__
#define __SIMPLESSD_HIL_HIL_HH__

#include <utility>

#include "hil/common/dma_engine.hh"
#include "icl/icl.hh"
#include "sim/object.hh"

namespace SimpleSSD::HIL {

class HIL;

enum class FormatOption : uint8_t {
  /* Format NVM */
  None,                //!< Invalidates FTL mapping (TRIM)
  UserDataErase,       //!< Erase block, by copying unaffected pages
  CryptographicErase,  //!< Not supported (Same as UserDataErase)

  /* Sanitize */
  BlockErase = UserDataErase,
  CryptoErase = CryptographicErase,
  Overwrite  //!< Overwrite data with provided 32bit pattern
};

enum class Operation : uint16_t {
  None,
  Flush,
  Read,
  Write,
  Format,
  Compare,
  CompareAndWrite,
  WriteZeroes,
};

class Request {
 private:
  friend HIL;

  DMAEngine *dmaEngine;
  DMATag dmaTag;

  Event eid;
  uint64_t data;

  uint64_t offset;  //!< Byte offset
  uint32_t length;  //!< Byte length

  uint16_t dmaCounter;
  uint16_t nvmCounter;
  uint16_t nlp;  //!< # logical pages

  Operation opcode;

  uint64_t dmaBeginAt;
  uint64_t nvmBeginAt;

 public:
  Request(Event e, uint64_t c)
      : dmaEngine(nullptr),
        dmaTag(InvalidDMATag),
        eid(e),
        data(c),
        offset(0),
        length(0),
        nlp(0),
        dmaCounter(0),
        nvmCounter(0),
        opcode(Operation::None),
        dmaBeginAt(0),
        nvmBeginAt(0) {}
  Request(DMAEngine *d, DMATag t, Event e, uint64_t c)
      : dmaEngine(d),
        dmaTag(t),
        eid(e),
        data(c),
        offset(0),
        length(0),
        nlp(0),
        dmaCounter(0),
        nvmCounter(0),
        opcode(Operation::None),
        dmaBeginAt(0),
        nvmBeginAt(0) {}

  void setAddress(LPN slpn, uint16_t nlp, uint32_t lbaSize) {
    offset = slpn * lbaSize;
    length = nlp * lbaSize;
  }

  void setAddress(uint64_t byteoffset, uint32_t bytelength) {
    offset = byteoffset;
    length = bytelength;
  }
};

/**
 * \brief HIL (Host Interface Layer) class
 *
 * Defines abstract layer to the cache layer. All SSD controllers use this class
 * to communicate with underlying NVM media.
 *
 * Provides five basic operations - read, write, flush, trim and format.
 * TRIM and format is simillar - both operation erases user data.
 *
 * Actually, this is not a HIL - a part of HIL.
 */
class HIL : public Object {
 private:
  ICL::ICL icl;

  uint64_t requestCounter;

  std::unordered_map<uint64_t, Request> requestQueue;

 public:
  HIL(ObjectData &);
  ~HIL();

  /**
   * \brief Read underlying NVM
   *
   * \param[in] req Request object
   */
  void read(Request &&req);

  /**
   * \brief Write underlying NVM
   *
   * If zerofill is true, DMAEngine and DMATag can be null.
   *
   * \param[in] req       Request object
   * \param[in] fua       Force Unit Access
   * \param[in] zerofill  Write zeroes
   */
  void write(Request &&req, bool fua = false, bool zerofill = false);

  /**
   * \brief Flush cache
   *
   * If cache is not enabled, this command has no effect.
   * DMAEngine and DMATag can be null.
   *
   * \param[in] req Request object
   */
  void flush(Request &&req);

  /**
   * \brief TRIM/Format NVM
   *
   * DMAEngine and DMATag can be null.
   *
   * \param[in] req     Request object
   * \param[in] option  Format option
   */
  void format(Request &&req, FormatOption option);

  /**
   * \brief Compare
   *
   * \param[in] req   Request object
   * \param[in] fused True if this request is FUSED operation in NVMe
   */
  void compare(Request &&req, bool fused = false);

  /**
   * \brief Get logical pages contains data
   *
   * To implement per-namespace bases utilization, this function requires offset
   * and length.
   */
  LPN getPageUsage(LPN offset, LPN length);

  //! Get total logical pages in current HIL object
  LPN getTotalPages();

  //! Get bytesize of one logical page.
  uint32_t getLPNSize();

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL

#endif
