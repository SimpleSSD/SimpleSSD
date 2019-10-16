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

#include "icl/icl.hh"
#include "sim/object.hh"

namespace SimpleSSD::HIL {

enum class FormatOption {
  /* Format NVM */
  None,                //!< Invalidates FTL mapping (TRIM)
  UserDataErase,       //!< Erase block, by copying unaffected pages
  CryptographicErase,  //<! Not supported (Same as UserDataErase)

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
  ICL::ICL *pICL;

  uint32_t requestCounter;

  uint32_t logicalPageSize;

 public:
  HIL(ObjectData &);
  HIL(const HIL &) = delete;
  HIL(HIL &&) noexcept = default;
  ~HIL();

  HIL &operator=(const HIL &) = delete;
  HIL &operator=(HIL &&) = default;

  /**
   * \brief Read logical pages
   *
   * \param[in]  offset   Offset in LPN to read
   * \param[in]  length   # of logical pages to read
   * \param[out] buffer   Array for retrived data (length * logical page size)
   * \param[in] unwritten Byte offset to exclude <first bytes, last bytes>
   * \param[in]  eid      Completion event
   * \param[in]  data     Data for event
   */
  void readPages(LPN offset, LPN length, uint8_t *buffer,
                 std::pair<uint32_t, uint32_t> &&unread, Event eid,
                 uint64_t data = 0);

  /**
   * \brief Write logical pages
   *
   * If logical block is smaller than logical page, first loglcal page and last
   * logical page may contains unwritten area. Then specify unwritten area using
   * unwritten parameter.
   *
   * \param[in] offset    Offset in LPN to write
   * \param[in] length    # of logical pages to write
   * \param[in] buffer    Array for data to write (length * logical page size)
   * \param[in] unwritten Byte offset to exclude <first bytes, last bytes>
   * \param[in] eid       Completion event
   * \param[in] data      Data for event
   */
  void writePages(LPN offset, LPN length, uint8_t *buffer,
                  std::pair<uint32_t, uint32_t> &&unwritten, Event eid,
                  uint64_t data = 0);

  /**
   * \brief Flush cache
   *
   * To implement per-namespace cache flush, this function requires offset and
   * length.
   *
   * \param[in] offset  Offset in LPN to flush
   * \param[in] length  # of logical pages to flush
   * \param[in] eid     Completion event
   * \param[in] data    Data for event
   */
  void flushCache(LPN offset, LPN length, Event eid, uint64_t data = 0);

  /**
   * \brief Trim logical pages
   *
   * \param[in] offset  Offset in LPN to trim
   * \param[in] length  # of logical pages to trim
   * \param[in] eid     Completion event
   * \param[in] data    Data for event
   */
  void trimPages(LPN offset, LPN length, Event eid, uint64_t data = 0);

  /**
   * \brief Format logical pages
   *
   * To implement per-namespace format, this function requires offset and
   * length.
   *
   * \param[in] offset  Offset in LPN to format
   * \param[in] length  # of logical pages to format
   * \param[in] option  Format method to use
   * \param[in] eid     Completion event
   * \param[in] data    Data for event
   */
  void formatPages(LPN offset, LPN length, FormatOption option, Event eid,
                   uint64_t data = 0);

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
  uint64_t getLPNSize();

  //! Enable/disable ICL
  void setCache(bool);

  //! Get cache enabled
  bool getCache();

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL

#endif
