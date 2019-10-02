// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/nvme/dma_engine.hh"

namespace SimpleSSD::HIL::NVMe {

PRPEngine::PRP::PRP() : address(0), size(0) {}

PRPEngine::PRP::PRP(uint64_t a, uint64_t s) : address(a), size(s) {}

PRPEngine::PRPEngine(ObjectData &&o, Interface *i, uint64_t p)
    : DMAEngine(std::move(o), i), inited(false), totalSize(0), pageSize(p) {
  panic_if(popcount(p) != 1, "Invalid memory page size provided.");

  readPRPList = createEvent(
      [this](uint64_t t, void *c) {
        getPRPListFromPRP_readDone(t, (PRPInitContext *)c);
      },
      "HIL::NVMe::PRPEngine::readPRPList");
}

PRPEngine::~PRPEngine() {}

void PRPEngine::getPRPListFromPRP_readDone(uint64_t now, PRPInitContext *data) {
  uint64_t listPRP;
  uint64_t listPRPSize;

  for (size_t i = 0; i < data->bufferSize; i += 8) {
    listPRP = *((uint64_t *)(data->buffer + i));
    listPRPSize = getSizeFromPRP(listPRP);

    if (listPRP == 0) {
      panic("prp_list: Invalid PRP in PRP List");
    }

    data->handledSize += listPRPSize;
    prpList.push_back(PRP(listPRP, listPRPSize));

    if (data->handledSize >= totalSize) {
      break;
    }
  }

  free(data->buffer);

  if (data->handledSize < totalSize) {
    // PRP list ends but size is not full
    // Last item of PRP list is pointer of another PRP list
    listPRP = prpList.back().address;
    listPRPSize = getSizeFromPRP(listPRP);

    prpList.pop_back();

    data->bufferSize = listPRPSize;
    data->buffer = (uint8_t *)malloc(data->bufferSize);

    pInterface->read(listPRP, data->bufferSize, data->buffer, readPRPList,
                     data);
  }
  else {
    // Done
    inited = true;

    schedule(data->eid, now, data->context);

    delete data;
  }
}

uint64_t PRPEngine::getSizeFromPRP(uint64_t prp) {
  return pageSize - (prp & (pageSize - 1));
}

void PRPEngine::getPRPListFromPRP(uint64_t prp, Event eid, void *context) {
  auto data = new PRPInitContext();

  data->handledSize = 0;
  data->eid = eid;
  data->context = context;
  data->bufferSize = getSizeFromPRP(prp);
  data->buffer = (uint8_t *)malloc(data->bufferSize);

  pInterface->read(prp, data->bufferSize, data->buffer, readPRPList, data);
}

void PRPEngine::initData(uint64_t prp1, uint64_t prp2, uint64_t sizeLimit,
                         Event eid, void *context) {
  bool immediate = true;
  uint8_t mode = 0xFF;
  uint64_t prp1Size = getSizeFromPRP(prp1);
  uint64_t prp2Size = getSizeFromPRP(prp2);

  totalSize = sizeLimit;

  // Determine PRP1 and PRP2
  if (totalSize <= pageSize) {
    if (totalSize <= prp1Size) {
      mode = 0;
    }
    else {
      mode = 1;
    }
  }
  else if (totalSize <= pageSize * 2) {
    if (prp1Size == pageSize) {
      mode = 1;
    }
    else {
      mode = 2;
    }
  }
  else {
    mode = 2;
  }

  switch (mode) {
    case 0:
      // PRP1 is PRP pointer, PRP2 is not used
      prpList.push_back(PRP(prp1, totalSize));

      break;
    case 1:
      // PRP1 and PRP2 are PRP pointer
      prpList.push_back(PRP(prp1, prp1Size));
      prpList.push_back(PRP(prp2, prp2Size));

      panic_if(prp1Size + prp2Size < totalSize, "Invalid DPTR size");

      break;
    case 2:
      immediate = false;

      // PRP1 is PRP pointer, PRP2 is PRP list
      prpList.push_back(PRP(prp1, prp1Size));
      getPRPListFromPRP(prp2, eid, context);

      break;
    default:
      panic("Invalid PRP1/2 while parsing.");

      break;
  }

  if (immediate) {
    inited = true;

    schedule(eid, getTick(), context);
  }
}

void PRPEngine::initQueue(uint64_t base, uint64_t size, bool cont, Event eid,
                          void *context) {
  totalSize = size;

  if (cont) {
    prpList.push_back(PRP(base, size));

    inited = true;
    schedule(eid, getTick(), context);
  }
  else {
    getPRPListFromPRP(base, eid, context);
  }
}

}  // namespace SimpleSSD::HIL::NVMe
