//
//  FTL.cpp
//  FTL-3
//
//  Created by Narges on 7/3/15.
//  Copyright (c) 2015 narges shahidi. All rights reserved.
//
//  Modified by Donghyun Gouk <kukdh1@camelab.org>
//

#include "ftl.hh"
#include "ftl_statistics.hh"
#include "pal/old/PAL2.h"

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

FTL::FTL(Parameter *p, SimpleSSD::PAL::PAL *l) : param(p), pal(l) {
  FTLmapping = new HybridMapping(this);
}

FTL::~FTL() {
  delete param;
}

bool FTL::initialize() {
  SimpleSSD::FTL::Request req(param->ioUnitInPage);

  req.ioFlag.set();

  std::cout << "Total physical block/page " << param->physical_block_number
            << "  " << param->physical_page_number << endl;
  std::cout << "Total logical block/page " << param->logical_block_number
            << "  " << param->logical_page_number << endl;

  for (Addr i = 0; i < param->logical_block_number; i++) {
    int to_fill_page_number = (param->page_per_block * param->warmup);
    if (to_fill_page_number > param->page_per_block) {
      cout << "error in initialization " << endl;
      return false;
    }
    if (to_fill_page_number != 0) {
      for (int j = 0; j < to_fill_page_number; j++) {
        req.lpn = i * param->page_per_block + j;

        write(req, 0, true);
      }
    }
  }
  std::cout << "Initialization done! " << std::endl;

  return true;
}

Tick FTL::read(SimpleSSD::FTL::Request &req, Tick arrived) {
  Tick finished = 0;
  Tick oneReq;
  Addr ppn;
  SimpleSSD::PAL::Request palRequest(req);

  ftl_statistics.read_req_arrive(arrived);

  FTLmapping->read(req.lpn, ppn);
  palRequest.blockIndex = ppn / param->page_per_block;
  palRequest.pageIndex = ppn % param->page_per_block;
  oneReq = readInternal(palRequest, arrived);
  finished = MAX(oneReq, finished);

  Command cmd = Command(arrived, req.lpn, OPER_READ, param->page_byte);
  cmd.finished = finished;

  ftl_statistics.updateStats(&cmd);

  return finished;
}

Tick FTL::write(SimpleSSD::FTL::Request &req, Tick arrived, bool init) {
  Tick finished = 0;
  Tick oneReq;
  Addr ppn;
  SimpleSSD::PAL::Request palRequest(req);

  ftl_statistics.write_req_arrive(arrived);

  FTLmapping->write(req.lpn, ppn, arrived);
  if (!init) {
    palRequest.blockIndex = ppn / param->page_per_block;
    palRequest.pageIndex = ppn % param->page_per_block;
    oneReq = writeInternal(palRequest, arrived);
    finished = MAX(oneReq, finished);
  }

  if (FTLmapping->need_gc()) {
    FTLmapping->GarbageCollection(finished);
  }

  if (!init) {
    Command cmd = Command(arrived, req.lpn, OPER_WRITE, param->page_byte);
    cmd.finished = finished;

    ftl_statistics.updateStats(&cmd);
  }

  return finished;
}

Tick FTL::trim(SimpleSSD::FTL::Request &req) {
  // FIXME: Not implemented
  return 0;
}

Tick FTL::readInternal(SimpleSSD::PAL::Request &req, Tick now, bool flag) {
  pal->read(req, now);

  return now;
}

Tick FTL::writeInternal(SimpleSSD::PAL::Request &req, Tick now, bool flag) {
  pal->write(req, now);

  return now;
}

Tick FTL::eraseInternal(SimpleSSD::PAL::Request &req, Tick now) {
  pal->erase(req, now);

  return now;
}

void FTL::PrintStats(Tick sim_time) {
  // release finished requests
  ftl_statistics.print_epoch_stats(sim_time);
  FTLmapping->PrintStats();
}

void FTL::PrintFinalStats(Tick sim_time) {
  // que->PrintStats();
  // FTLmapping->PrintStats();
  ftl_statistics.print_final_stats(sim_time);
}
