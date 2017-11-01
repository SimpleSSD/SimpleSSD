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

#include "ftl_hybridmapping.hh"
#include "ftl_mappingtable.hh"
#include "ftl_defs.hh"
#include "ftl_statistics.hh"
#include "ftl_command.hh"

#define __STDC_FORMAT_MACROS 1

#include <inttypes.h>
#include <cinttypes>
#include <cfloat>
#include <ctime>

class PAL2;

class FTL {
  protected:
    Parameter *param;

    PAL2 *pal;

  public:
    FTL(Parameter *, PAL2 *);
    ~FTL();

    FTLStats ftl_statistics;
    MappingTable *FTLmapping;

    bool initialize();
    Parameter * getParameter(){return param;}

    Tick read(Addr lpn, size_t npages, Tick arrived);
    Tick write(Addr lpn, size_t npages, Tick arrived, bool init = false);
    Tick trim(Addr lpn, size_t npages);

    void translate(Addr lpn, CPDPBP *pa);

    void PrintStats(Tick sim_time);
    void PrintFinalStats(Tick sim_time);

    Tick readInternal(Addr ppn, Tick now, bool flag = false);
    Tick writeInternal(Addr ppn, Tick now, bool flag = false);
    Tick eraseInternal(Addr ppn, Tick now);
};

#endif /* defined(__FTL_3__FTL__) */
