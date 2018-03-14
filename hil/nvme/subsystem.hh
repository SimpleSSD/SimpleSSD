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

#ifndef __HIL_NVME_SUBSYSTEM__
#define __HIL_NVME_SUBSYSTEM__

#include "hil/hil.hh"
#include "hil/nvme/namespace.hh"

namespace SimpleSSD {

namespace HIL {

namespace NVMe {

class Controller;

class Subsystem : public StatObject {
 private:
  Controller *pParent;
  HIL *pHIL;

  std::list<Namespace *> lNamespaces;

  ConfigData *pCfgdata;
  Config &conf;
  uint32_t queueAllocated;

  HealthInfo globalHealth;
  uint32_t logicalPageSize;
  uint64_t totalLogicalPages;
  uint64_t allocatedLogicalPages;

  // Stats
  uint64_t commandCount;

  bool createNamespace(uint32_t, Namespace::Information *);
  bool destroyNamespace(uint32_t);
  void fillIdentifyNamespace(uint8_t *, Namespace::Information *);

  // Admin commands
  bool deleteSQueue(SQEntryWrapper &, CQEntryWrapper &, uint64_t &);
  bool createSQueue(SQEntryWrapper &, CQEntryWrapper &, uint64_t &);
  bool getLogPage(SQEntryWrapper &, CQEntryWrapper &, uint64_t &);
  bool deleteCQueue(SQEntryWrapper &, CQEntryWrapper &, uint64_t &);
  bool createCQueue(SQEntryWrapper &, CQEntryWrapper &, uint64_t &);
  bool identify(SQEntryWrapper &, CQEntryWrapper &, uint64_t &);
  bool abort(SQEntryWrapper &, CQEntryWrapper &, uint64_t &);
  bool setFeatures(SQEntryWrapper &, CQEntryWrapper &, uint64_t &);
  bool getFeatures(SQEntryWrapper &, CQEntryWrapper &, uint64_t &);
  bool asyncEventReq(SQEntryWrapper &, CQEntryWrapper &, uint64_t &);
  bool namespaceManagement(SQEntryWrapper &, CQEntryWrapper &, uint64_t &);
  bool namespaceAttachment(SQEntryWrapper &, CQEntryWrapper &, uint64_t &);
  bool formatNVM(SQEntryWrapper &, CQEntryWrapper &, uint64_t &);

 public:
  Subsystem(Controller *, ConfigData *);
  ~Subsystem();

  bool submitCommand(SQEntryWrapper &, CQEntryWrapper &, uint64_t &);
  void getNVMCapacity(uint64_t &, uint64_t &);
  uint32_t validNamespaceCount();

  void read(Namespace *, uint64_t, uint64_t, uint64_t &);
  void write(Namespace *, uint64_t, uint64_t, uint64_t &);
  void flush(Namespace *, uint64_t &);
  void trim(Namespace *, uint64_t, uint64_t, uint64_t &);

  void getStats(std::vector<Stats> &) override;
  void getStatValues(std::vector<uint64_t> &) override;
  void resetStats() override;
};

}  // namespace NVMe

}  // namespace HIL

}  // namespace SimpleSSD

#endif
