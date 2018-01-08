//
//  Defs.h
//  FTLSim_functional
//
//  Created by Narges on 6/24/15.
//  Copyright (c) 2015 narges shahidi. All rights reserved.
//

#ifndef FTLSim_functional_Defs_h
#define FTLSim_functional_Defs_h

#include <assert.h>
#include <stdint.h>
#include <fstream>
#include <iostream>
#include <istream>
#include "util/old/SimpleSSD_types.h"

#define EPOCH_INTERVAL 100000000000

enum STATE { ERROR = -1, FAIL, SUCCESS, RESERVED };

inline void my_assert(const char message[]) {
  printf("FTL: ERROR %s \n", message);
}

class Parameter {
 public:
  int page_per_block;
  uint32_t ioUnitInPage;
  Addr physical_page_number;
  Addr logical_page_number;
  Addr physical_block_number;
  Addr logical_block_number;
  int mapping_N;
  int mapping_K;
  double gc_threshold;
  int page_size;  // sector per page
  double over_provide;
  double warmup;
  int erase_cycle;
  size_t page_byte;
  void to_string() {
    std::cout << std::endl;
    std::cout << "Start simulation for N: " << mapping_N
              << " and K: " << mapping_K << std::endl;
    std::cout << "  page/block=" << page_per_block
              << " over-provisioning rate: " << over_provide << std::endl
              << std::endl;
  }
};

#endif
