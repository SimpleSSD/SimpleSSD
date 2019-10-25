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

#include "hil/command_manager.hh"
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
  CommandManager commandManager;
  ICL::ICL icl;

  uint64_t requestCounter;
  uint32_t logicalPageSize;

 public:
  HIL(ObjectData &);
  HIL(const HIL &) = delete;
  HIL(HIL &&) noexcept = default;
  ~HIL();

  HIL &operator=(const HIL &) = delete;
  HIL &operator=(HIL &&) = default;

  /**
   * \brief Submit command
   *
   * Command should be inserted through CommandManager before call this
   * function.
   *
   * \param[in] tag Unique command tag
   */
  void submitCommand(uint64_t tag);

  /**
   * \brief Submit command
   *
   * Command should be inserted through CommandManager before call this
   * function. Only begins specified subcommad.
   *
   * \param[in] tag Unique command tag
   * \param[in] id  Unique subcommand id
   */
  void submitSubcommand(uint64_t tag, uint32_t id);

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

  //! Enable/disable ICL
  void setCache(bool);

  //! Get cache enabled
  bool getCache();

  //! Get command manager
  CommandManager *getCommandManager();

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL

#endif
