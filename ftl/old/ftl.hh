//
//  FTL.h
//  FTL-3
//
//  Created by Narges on 7/3/15.
//  Copyright (c) 2015 narges shahidi. All rights reserved.
//
//  Modified by Donghyun Gouk <kukdh1@camelab.org>
//

#ifndef __FTL_3__FTL__
#define __FTL_3__FTL__

#include "util/old/SimpleSSD_types.h"

#include "ftl_command.hh"
#include "ftl_defs.hh"
#include "ftl_hybridmapping.hh"
#include "ftl_mappingtable.hh"
#include "ftl_statistics.hh"
#include "pal/pal.hh"
#include "util/def.hh"

#define __STDC_FORMAT_MACROS 1

#include <inttypes.h>
#include <cfloat>
#include <cinttypes>
#include <ctime>

class FTL {
 protected:
  Parameter *param;

  SimpleSSD::PAL::PAL *pal;

 public:
  FTL(Parameter *, SimpleSSD::PAL::PAL *);
  ~FTL();

  FTLStats ftl_statistics;
  MappingTable *FTLmapping;

  bool initialize();
  Parameter *getParameter() { return param; }

  Tick read(SimpleSSD::FTL::Request &, Tick arrived);
  Tick write(SimpleSSD::FTL::Request &, Tick arrived, bool init = false);
  Tick trim(SimpleSSD::FTL::Request &);

  void PrintStats(Tick sim_time);
  void PrintFinalStats(Tick sim_time);

  Tick readInternal(SimpleSSD::PAL::Request &, Tick now, bool flag = false);
  Tick writeInternal(SimpleSSD::PAL::Request &, Tick now, bool flag = false);
  Tick eraseInternal(SimpleSSD::PAL::Request &, Tick now);
};

#endif /* defined(__FTL_3__FTL__) */
