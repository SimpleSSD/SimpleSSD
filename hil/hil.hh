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

#include "hil/convert.hh"
#include "hil/request.hh"
#include "icl/icl.hh"
#include "sim/object.hh"

namespace SimpleSSD::HIL {

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
  ConvertFunction convertFunction;

  uint32_t lpnSize;

  uint64_t requestCounter;
  uint64_t subrequestCounter;

  std::unordered_map<uint64_t, Request *> requestQueue;
  std::unordered_map<uint64_t, SubRequest> subrequestQueue;

  void submit(Operation, Request *);

  Event eventNVMCompletion;
  void nvmCompletion(uint64_t, uint64_t);

  Event eventDMACompletion;
  void dmaCompletion(uint64_t, uint64_t);

 public:
  HIL(ObjectData &);
  ~HIL();

  /**
   * \brief Read underlying NVM
   *
   * \param[in] req Request object
   */
  void read(Request *req);

  /**
   * \brief Write underlying NVM
   *
   * If zerofill is true, DMAEngine and DMATag can be null.
   *
   * \param[in] req       Request object
   * \param[in] zerofill  Write zeroes
   */
  void write(Request *req, bool zerofill = false);

  /**
   * \brief Flush cache
   *
   * If cache is not enabled, this command has no effect.
   * DMAEngine and DMATag can be null.
   *
   * \param[in] req Request object
   */
  void flush(Request *req);

  /**
   * \brief TRIM/Format NVM
   *
   * DMAEngine and DMATag can be null.
   *
   * \param[in] req     Request object
   * \param[in] option  Format option
   */
  void format(Request *req, FormatOption option);

  /**
   * \brief Compare
   *
   * \param[in] req   Request object
   * \param[in] fused True if this request is FUSED operation in NVMe
   */
  void compare(Request *req, bool fused = false);

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
