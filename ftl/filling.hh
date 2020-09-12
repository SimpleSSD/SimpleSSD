// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_FILLING_HH__
#define __SIMPLESSD_FTL_FILLING_HH__

#include "ftl/mapping/abstract_mapping.hh"

namespace SimpleSSD::FTL {

class AbstractFTL;

class Filling : public Object {
 private:
  FTLObjectData &ftlobject;

 public:
  Filling(ObjectData &, FTLObjectData &);

  void start() noexcept;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;
  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL

#endif
