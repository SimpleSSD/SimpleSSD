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

#ifndef __HIL_NVME_DMA__
#define __HIL_NVME_DMA__

#include <vector>

#include "hil/nvme/config.hh"
#include "hil/nvme/def.hh"
#include "hil/nvme/interface.hh"
#include "util/config.hh"

namespace SimpleSSD {

namespace HIL {

namespace NVMe {

typedef struct {
  ConfigReader *pConfigReader;
  Interface *pInterface;
  uint64_t memoryPageSize;
  uint8_t memoryPageSizeOrder;
  uint16_t maxQueueEntry;
} ConfigData;

class DMAInterface {
 protected:
  Interface *pInterface;

 public:
  DMAInterface(ConfigData *);
  virtual ~DMAInterface();

  virtual uint64_t read(uint64_t, uint64_t, uint8_t *, uint64_t &) = 0;
  virtual uint64_t write(uint64_t, uint64_t, uint8_t *, uint64_t &) = 0;
};

struct PRP {
  uint64_t addr;
  uint64_t size;

  PRP();
  PRP(uint64_t, uint64_t);
};

class PRPList : public DMAInterface {
 private:
  std::vector<PRP> prpList;
  uint64_t totalSize;
  uint64_t pagesize;

  void getPRPListFromPRP(uint64_t, uint64_t);
  uint64_t getPRPSize(uint64_t);

 public:
  PRPList(ConfigData *, uint64_t, uint64_t, uint64_t);
  PRPList(ConfigData *, uint64_t, uint64_t, bool);
  ~PRPList();

  uint64_t read(uint64_t, uint64_t, uint8_t *, uint64_t &) override;
  uint64_t write(uint64_t, uint64_t, uint8_t *, uint64_t &) override;
};

union SGLDescriptor {
  uint8_t data[16];
  struct {
    uint64_t address;
    uint32_t length;
    uint8_t reserved[3];
    uint8_t id;
  };

  SGLDescriptor();
};

struct Chunk {
  uint64_t addr;
  uint32_t length;

  bool ignore;

  Chunk();
  Chunk(uint64_t, uint32_t, bool);
};

class SGL : public DMAInterface {
 private:
  std::vector<Chunk> list;
  uint64_t totalSize;

  void parseSGLDescriptor(SGLDescriptor &);
  void parseSGLSegment(uint64_t, uint32_t);

 public:
  SGL(ConfigData *, uint64_t, uint64_t);
  ~SGL();

  uint64_t read(uint64_t, uint64_t, uint8_t *, uint64_t &) override;
  uint64_t write(uint64_t, uint64_t, uint8_t *, uint64_t &) override;
};

}  // namespace NVMe

}  // namespace HIL

}  // namespace SimpleSSD

#endif
