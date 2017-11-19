//
//  Block.cpp
//  FTLSim_functional
//
//  Created by Narges on 6/24/15.
//  Copyright (c) 2015 narges shahidi. All rights reserved.
//
#include "ftl_block.hh"
#include "ftl_defs.hh"

inline int change_bit(int bitfield, int offset, int value) {
  bitfield =
      (bitfield | (1 << offset)) & (~((1 << offset) ^ (value << offset)));
  return bitfield;
}

void Block::initialize(int page_number, Addr bn) {
  block_number = bn;
  page_per_block = page_number;

  page_sequence_number = 0;
  erase_count = 0;
  bad_block = false;

  /* page_bit_map shows the state of pages valid or invalid.
   * if we have 256 pages, within the block, we need 256 bit
   * 256/32=8, so 8 (32-bit)integer. we add one more bit to
   * support blocks with non-power-of-two number of pages.
   */

  int temp = (page_per_block / INT_SIZE) + 1;
  page_bit_map = new unsigned int[temp];

  /* initialize page_bit_map: we can simply assign all
   * integers to zero if PAGE_VALID is zero.
   */

  for (int i = 0; i < temp; i++) {
    for (int j = 0; j < INT_SIZE; j++) {
      page_bit_map[i] = change_bit(page_bit_map[i], j, PAGE_VALID);
    }
  }
}
void Block::erase_block() {
  erase_count++;
  page_sequence_number = 0;

  int temp = (page_per_block / INT_SIZE) + 1;
  for (int i = 0; i < temp; i++) {
    for (int j = 0; j < INT_SIZE; j++) {
      page_bit_map[i] = change_bit(page_bit_map[i], j, PAGE_VALID);
    }
  }
}
void Block::set_page_state(int page_offset, PAGE_STATE state) {
  if (page_offset > page_sequence_number) {
    return;
  }
  int index = page_offset / INT_SIZE;
  int bit_offset = page_offset % INT_SIZE;

  page_bit_map[index] = change_bit(page_bit_map[index], bit_offset, state);
}
int Block::get_page_state(int page_offset) {
  if (page_offset >= page_per_block)
    return ERROR;
  if (page_offset >= page_sequence_number)
    return PAGE_FREE;

  int index = page_offset / INT_SIZE;
  int bit_offset = page_offset = page_offset % INT_SIZE;

  int flag = 1 << bit_offset;  // flag = 0000..0001000...000

  if ((flag & page_bit_map[index]) == PAGE_VALID)
    return PAGE_VALID;
  else
    return PAGE_INVALID;
}

bool Block::is_empty() {
  if (page_sequence_number == 0)
    return true;
  return false;
}
bool Block::is_full() {
  if (page_sequence_number >= page_per_block)
    return true;
  return false;
}
STATE Block::write_page(Addr logical_page, int &page_offset) {
  if (is_full()) {
    my_assert("write into full block");
    return ERROR;
  }

  if (page_offset == -1) {  // No specific page offset has passed.
    page_offset = page_sequence_number++;
    return SUCCESS;
  }

  if (get_page_state(page_offset) != PAGE_FREE) {
    my_assert("write into non-free page");
    return ERROR;
  }
  if (page_offset == page_sequence_number) {
    page_offset = page_sequence_number++;
    return SUCCESS;
  }

  if (page_offset < page_sequence_number) {
    my_assert("non-sequential write into the block ");
    return ERROR;
  }
  else {
    while (page_sequence_number < page_offset) {
      set_page_state(page_sequence_number, PAGE_INVALID);
      page_sequence_number++;
    }
  }

  page_offset = page_sequence_number++;
  return SUCCESS;
}
int Block::valid_page_count() {
  int count = 0;
  for (int i = 0; i < page_sequence_number; i++) {
    if (get_page_state(i) == PAGE_VALID) {
      count++;
    }
  }
  return count;
}
int Block::free_page_count() {
  return page_per_block - page_sequence_number;
}
void Block::to_string() {
  std::cout << "{";
  for (int i = 0; i < page_per_block; i++) {
    int s = get_page_state(i);
    if (s == PAGE_INVALID) {
      std::cout << " I ,";
    }
    else {
      if (i < page_sequence_number)
        std::cout << " V ,";
      else
        std::cout << " F ,";
    }
  }
  std::cout << "}" << std::endl;

  std::cout << "erase count for block " << block_number << ": " << erase_count
            << std::endl;
}
