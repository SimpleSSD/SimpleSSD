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

#ifndef __HIL_NVME_CONFIG__
#define __HIL_NVME_CONFIG__

#include "util/base_config.hh"

namespace SimpleSSD {

namespace HIL {

namespace NVMe {

typedef enum {
  NVME_WORK_INTERVAL,
  NVME_MAX_REQUEST_COUNT,
  NVME_MAX_IO_CQUEUE,
  NVME_MAX_IO_SQUEUE,
  NVME_WRR_HIGH,
  NVME_WRR_MEDIUM,
  NVME_ENABLE_DEFAULT_NAMESPACE,
  NVME_LBA_SIZE,
  NVME_ENABLE_DISK_IMAGE,
  NVME_STRICT_DISK_SIZE,
  NVME_DISK_IMAGE_PATH,
  NVME_USE_COW_DISK
} NVME_CONFIG;

class Config : public BaseConfig {
 private:
  uint64_t workInterval;        //!< Default: 50000 (50ns)
  uint64_t maxRequestCount;     //!< Default: 4
  uint16_t maxIOCQueue;         //!< Default: 16
  uint16_t maxIOSQueue;         //!< Default: 16
  uint16_t wrrHigh;             //!< Default: 2
  uint16_t wrrMedium;           //!< Default: 2
  uint64_t lbaSize;             //!< Default: 512
  bool enableDefaultNamespace;  //!< Default: True
  bool enableDiskImage;         //!< Default: False
  bool strictDiskSize;          //!< Default: False
  bool useCopyOnWriteDisk;      //!< Default: False
  std::string diskImagePath;    //!< Default: ""

 public:
  Config();

  bool setConfig(const char *, const char *) override;
  void update() override;

  int64_t readInt(uint32_t) override;
  uint64_t readUint(uint32_t) override;
  std::string readString(uint32_t) override;
  bool readBoolean(uint32_t) override;
};

}  // namespace NVMe

}  // namespace HIL

}  // namespace SimpleSSD

#endif
