//
//  Block.h
//  FTLSim_functional
//
//  Created by Narges on 6/24/15.
//  Copyright (c) 2015 narges shahidi. All rights reserved.
//

#ifndef FTLSim_functional_Block_h
#define FTLSim_functional_Block_h

#include <iostream>

#include "ftl/old/ftl_defs.hh"

#define INT_SIZE 32

class Block {
 private:
 public:
  enum PAGE_STATE {
    PAGE_VALID = 0,
    PAGE_INVALID,
    PAGE_FREE
  };                   // since page_bit_map is assigning one bit for each
                       // page, state should be only zero or one
  int page_per_block;  // total number of page per block
  uint64_t block_number;   // physical block number
  int erase_count;     // number of erase count on the block
  unsigned int
      *page_bit_map;  // each bit shows the state of a page
                      // value for each bit: (PAGE_VALID vs. PAGE_INVALID)
                      // since each block has more than 32 (int size) bit,
                      // an array of integer is used
  uint64_t page_sequence_number;  // page within the block have to be written in
                              // order
  bool bad_block;             // true: bad_block false:good_block

  Block() {}
  void initialize(int page_per_block,
                  uint64_t bn);  // initialize page_bit_map, etc.
  void
  erase_block();  // add erase count, reset other variables to initial state
  void set_page_state(
      int page_offset,
      PAGE_STATE state);  // change page state: PAGE_VALID or PAGE_INVALID
  int get_page_state(
      int page_offset);  // get the page state, return PAGE_VALID or
                         // PAGE_INVALID or PAGE_FREE
  bool is_empty();       // return true if block is empty, otherwise, false.
  bool is_full();        // return true if no free page remained in the block
  STATE write_page(uint64_t logical_page,
                   int &page_offset);  // write a new page into the block.
                                       // if page_offset == -1: write using
                                       // page_sequence_number otherwise,
                                       // invalid all free pages until reach to
                                       // this page
  int valid_page_count();  // return the number of valid pages within the block
  int free_page_count();   // return number of free pages inside the block
  void to_string();        // some print form of the block (use for debug)
};

#endif
