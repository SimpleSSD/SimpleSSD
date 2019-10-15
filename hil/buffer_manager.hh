// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_BUFFER_MANAGER_HH__
#define __SIMPLESSD_HIL_BUFFER_MANAGER_HH__

#include <cinttypes>
#include <utility>
#include <vector>

#include "sim/object.hh"

namespace SimpleSSD::HIL {

/**
 * \brief Buffer Manager class
 *
 * This manager exists for checkpointing I/O buffer passed to ICL/FTL and FIL.
 * As buffer itself created in HIL and the contents of buffer is checkpointed
 * in HIL, ICL/FTL and FIL only maintains pointer value.
 * After restore, there should be a way to convert old pointer value to new one.
 *
 * This buffer manager records pointers when checkpointing and restore pointer
 * values to updated one.
 */
class BufferManager : public Object {
 private:
  std::vector<std::pair<uint8_t *, uint64_t>> oldList;
  std::vector<std::pair<uint8_t *, uint64_t>> newList;

 public:
  BufferManager(ObjectData &);
  BufferManager(const BufferManager &) = delete;
  BufferManager(BufferManager &&) noexcept = default;

  BufferManager &operator=(const BufferManager &) = delete;
  BufferManager &operator=(BufferManager &&) = default;

  void registerPointer(std::ostream &, uint8_t *, uint64_t) const;
  void updatePointer(std::istream &, uint8_t *, uint64_t);
  uint8_t *restorePointer(uint8_t *);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL

#endif
