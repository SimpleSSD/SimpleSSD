// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __UTIL_ABSTRACT_FIFO_HH__
#define __UTIL_ABSTRACT_FIFO_HH__

#include <queue>

#include "sim/object.hh"

namespace SimpleSSD {

class AbstractFIFO : public Object {
 protected:
  bool readPending;
  bool writePending;

  Event eventReadDone;
  Event eventWriteDone;

  std::queue<void *> readQueue;
  std::queue<void *> writeQueue;

  void submitRead();
  void readDone(void *);
  void submitWrite();
  void writeDone(void *);

 public:
  AbstractFIFO(ObjectData &o);
  virtual ~AbstractFIFO();

  void read(void *);
  void write(void *);

  virtual uint64_t preSubmitRead(void *);
  virtual uint64_t preSubmitWrite(void *);
  virtual void postReadDone(void *);
  virtual void postWriteDone(void *);

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint() noexcept override;
  void restoreCheckpoint() noexcept override;
};

}  // namespace SimpleSSD

#endif
