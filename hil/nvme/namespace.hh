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

#ifndef __HIL_NVME_NAMESPACE__
#define __HIL_NVME_NAMESPACE__

#include <list>

#include "hil/hil.hh"
#include "hil/nvme/def.hh"
#include "hil/nvme/dma.hh"
#include "hil/nvme/queue.hh"
#include "util/disk.hh"

namespace SimpleSSD {

namespace HIL {

namespace NVMe {

class Subsystem;

typedef union _DatasetManagementRange {
  uint8_t data[0x10];
  struct {
    uint32_t attr;
    uint32_t nlb;
    uint64_t slba;
  };
} DatasetManagementRange;

class Namespace {
 public:
  typedef struct _Information {
    uint64_t size;                         //!< NSZE
    uint64_t capacity;                     //!< NCAP
    uint64_t utilization;                  //!< NUSE
    uint64_t sizeInByteL;                  //<! NVMCAPL
    uint64_t sizeInByteH;                  //<! NVMCAPH
    uint8_t lbaFormatIndex;                //!< FLBAS
    uint8_t dataProtectionSettings;        //!< DPS
    uint8_t namespaceSharingCapabilities;  //!< NMIC

    uint32_t lbaSize;
    LPNRange range;
  } Information;

 private:
  Subsystem *pParent;
  Disk *pDisk;

  ConfigData *pCfgdata;
  Config &conf;

  Information info;
  uint32_t nsid;
  bool attached;
  bool allocated;

  HealthInfo health;

  uint64_t formatFinishedAt;

  // Admin commands
  void getLogPage(SQEntryWrapper &, CQEntryWrapper &, uint64_t &);

  // NVM commands
  void flush(SQEntryWrapper &, CQEntryWrapper &, uint64_t &);
  void write(SQEntryWrapper &, CQEntryWrapper &, uint64_t &);
  void read(SQEntryWrapper &, CQEntryWrapper &, uint64_t &);
  void datasetManagement(SQEntryWrapper &, CQEntryWrapper &, uint64_t &);

 public:
  Namespace(Subsystem *, ConfigData *);
  ~Namespace();

  bool submitCommand(SQEntryWrapper &, CQEntryWrapper &, uint64_t &);

  void setData(uint32_t, Information *);
  void attach(bool);
  uint32_t getNSID();
  Information *getInfo();
  bool isAttached();

  void format(uint64_t);
};

}  // namespace NVMe

}  // namespace HIL

}  // namespace SimpleSSD

#endif
