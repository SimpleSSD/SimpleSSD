// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "sim/simplessd.hh"

namespace SimpleSSD {

//! SimpleSSD constructor
SimpleSSD::SimpleSSD() : inited(false) {}

//! SimpleSSD destructor
SimpleSSD::~SimpleSSD() {
  if (inited) {
    deinit();
  }
}

/**
 * \brief Initialize SimpleSSD
 *
 * Initialize all components of SSD. You must call this function before
 * call any methods of SimpleSSD object.
 *
 * \param[in]  config    SimpleSSD::Config object.
 * \param[in]  engine    SimpleSSD::Engine object.
 * \param[out] interface SimpleSSD::Interface object.
 * \return Initialization result.
 */
bool SimpleSSD::init() noexcept {
  return inited;
}

/**
 * \brief Deinitialize SimpleSSD
 *
 * Release all resources allocated for this object.
 */
void SimpleSSD::deinit() noexcept {}

}  // namespace SimpleSSD
