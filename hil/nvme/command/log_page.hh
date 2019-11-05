// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NVME_COMMAND_LOG_PAGE_HH__
#define __SIMPLESSD_HIL_NVME_COMMAND_LOG_PAGE_HH__

#include <set>

#include "sim/object.hh"

namespace SimpleSSD::HIL::NVMe {

class LogPage;

class ChangedNamespaceList : public Object {
 private:
  bool overflowed;

  std::set<uint32_t> list;

 public:
  ChangedNamespaceList(ObjectData &);

  void appendList(uint32_t);
  void makeResponse(uint64_t, uint64_t, uint8_t *);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

class LogPage : public Object {
 public:
  // See NVMe 1.4 Section 5.14 Figure 191/192
  // Log ID Description -> Supported by SimpleSSD?
  // 02h SMART/Health Information -> data.subsystem
  // 04h Changed Namespace List
  ChangedNamespaceList cnl;

  LogPage(ObjectData &);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
