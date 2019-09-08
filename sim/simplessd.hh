// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIM_SIMPLESSD_HH__
#define __SIM_SIMPLESSD_HH__

namespace SimpleSSD {

/**
 * \brief SimpleSSD object declaration
 *
 * SimpleSSD object. It contains everything about a SSD.
 * You can create multiple SimpleSSD objects to create multiple SSD.
 * You cannot copy this object.
 */
class SimpleSSD {
 private:
  bool inited;  //!< Flag whether this object is initialized

 public:
  SimpleSSD();
  SimpleSSD(const SimpleSSD &) = delete;
  SimpleSSD(SimpleSSD &&) = default;
  ~SimpleSSD();

  SimpleSSD &operator=(const SimpleSSD &) = delete;
  SimpleSSD &operator=(SimpleSSD &&) noexcept = default;

  bool init() noexcept;
  void deinit() noexcept;

  void createCheckpoint() noexcept;
  void restoreCheckpoint() noexcept;
};

}  // namespace SimpleSSD

#endif
