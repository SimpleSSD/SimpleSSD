/*
 * Copyright (C) 2017 CAMELab
 *
 * This file is part of SimpleSSD.
 *
 * SimpleSSD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SimpleSSD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SimpleSSD.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __HIL_NVME_CONTROLLER__
#define __HIL_NVME_CONTROLLER__

#include <list>
#include <unordered_map>

#include "hil/nvme/def.hh"
#include "hil/nvme/dma.hh"
#include "hil/nvme/interface.hh"
#include "hil/nvme/queue.hh"
#include "util/config.hh"
#include "util/def.hh"

namespace SimpleSSD {

namespace HIL {

namespace NVMe {

class Subsystem;

typedef union _RegisterTable {
  uint8_t data[64];
  struct {
    uint64_t capabilities;
    uint32_t version;
    uint32_t interruptMaskSet;
    uint32_t interruptMaskClear;
    uint32_t configuration;
    uint32_t reserved;
    uint32_t status;
    uint32_t subsystemReset;
    uint32_t adminQueueAttributes;
    uint64_t adminSQueueBaseAddress;
    uint64_t adminCQueueBaseAddress;
    uint32_t memoryBufferLocation;
    uint32_t memoryBufferSize;
  };

  _RegisterTable();
} RegisterTable;

typedef struct {
  uint64_t nextTime;
  uint32_t requestCount;
  bool valid;
  bool pending;
} AggregationInfo;

class Controller : public StatObject {
 private:
  Interface *pParent;     //!< NVMe::Interface passed from constructor
  Subsystem *pSubsystem;  //!< NVMe::Subsystem allocate in constructor

  RegisterTable registers;   //!< Table for NVMe Controller Registers
  uint64_t sqstride;         //!< Calculated SQ Stride
  uint64_t cqstride;         //!< Calculated CQ stride
  uint8_t adminQueueInited;  //!< Flag for initialization of Admin CQ/SQ
  uint16_t arbitration;      //!< Selected Arbitration Mechanism
  uint32_t interruptMask;    //!< Variable to store current interrupt mask

  CQueue **ppCQueue;  //!< Completion Queue array
  SQueue **ppSQueue;  //!< Submission Queue array

  std::list<SQEntryWrapper> lSQFIFO;  //!< Internal FIFO queue for submission
  std::list<CQEntryWrapper> lCQFIFO;  //!< Internal FIFO queue for completion

  bool shutdownReserved;

  uint64_t aggregationTime;
  uint32_t aggregationThreshold;
  std::unordered_map<uint16_t, AggregationInfo> aggregationMap;

  ConfigData cfgdata;
  Config &conf;

  static bool checkQueue(SQueue *, std::list<SQEntryWrapper> &, uint64_t &);
  void reserveCompletion();

 public:
  Controller(Interface *, ConfigReader *);
  ~Controller();

  void readRegister(uint64_t, uint64_t, uint8_t *, uint64_t &);
  void writeRegister(uint64_t, uint64_t, uint8_t *, uint64_t &);
  void ringCQHeadDoorbell(uint16_t, uint16_t, uint64_t &);
  void ringSQTailDoorbell(uint16_t, uint16_t, uint64_t &);

  void clearInterrupt(uint16_t);
  void updateInterrupt(uint16_t, bool);

  void submit(CQEntryWrapper &);
  void completion(uint64_t);
  int createCQueue(uint16_t, uint16_t, uint16_t, bool, bool, uint64_t);
  int createSQueue(uint16_t, uint16_t, uint16_t, uint8_t, bool, uint64_t);
  int deleteCQueue(uint16_t);
  int deleteSQueue(uint16_t);
  int abort(uint16_t, uint16_t);
  void identify(uint8_t *);
  void setCoalescingParameter(uint8_t, uint8_t);
  void getCoalescingParameter(uint8_t *, uint8_t *);
  void setCoalescing(uint16_t, bool);
  bool getCoalescing(uint16_t);

  void collectSQueue(uint64_t &);
  void work(uint64_t &);

  void getStats(std::vector<Stats> &) override;
  void getStatValues(std::vector<uint64_t> &) override;
  void resetStats() override;
};

}  // namespace NVMe

}  // namespace HIL

}  // namespace SimpleSSD

#endif
