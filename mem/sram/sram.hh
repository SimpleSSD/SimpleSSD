// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __MEM_SRAM_SRAM_HH__
#define __MEM_SRAM_SRAM_HH__

#include "mem/sram/abstract_sram.hh"

namespace SimpleSSD::Memory::SRAM {

class SRAM : public AbstractSRAM {
 protected:
  inline uint64_t preSubmit(Request *);
  inline void postDone(Request *);

  uint64_t preSubmitRead(void *) override;
  uint64_t preSubmitWrite(void *) override;
  void postReadDone(void *) override;
  void postWriteDone(void *) override;

 public:
  SRAM(ObjectData &);
  ~SRAM();

  void read(uint64_t, uint64_t, Event, void * = nullptr) override;
  void write(uint64_t, uint64_t, Event, void * = nullptr) override;
};

}  // namespace SimpleSSD::Memory::SRAM

#endif
