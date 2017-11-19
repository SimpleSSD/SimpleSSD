//
//  MappingTable.cpp
//  FTL-3
//
//  Created by Narges on 7/4/15.
//  Copyright (c) 2015 narges shahidi. All rights reserved.
//

#include "ftl.hh"
#include "ftl_mappingtable.hh"

MappingTable::MappingTable(FTL *f) : ftl(f) {
  param = f->getParameter();

  free_block_count = 0;
  map_free_block_count = 0;  // will be fixed to initial value in add_free_block
  map_used_block_count = param->physical_block_number;  // will be fixed later
  physical_blocks = new Block[param->physical_block_number];
  free_blocks = new Block *[param->physical_block_number];

  for (Addr i = 0; i < param->physical_block_number; i++) {
    physical_blocks[i].initialize(param->page_per_block, i);
    add_free_block(i);
  }

  gc_flag = false;
}

MappingTable::~MappingTable() {
  delete[] physical_blocks;
}

STATE MappingTable::get_free_block(Addr &free_block) {
  if (free_block_count == 0) {
    cout << "no free block " << endl;
    return FAIL;
  }
  Addr selected_block = free_blocks[0]->block_number;

  free_blocks[0] = free_blocks[free_block_count - 1];
  free_blocks[free_block_count - 1] = NULL;

  free_block_count--;
  map_free_block_count--;
  map_used_block_count++;

  int center = 0;
  int left = 1;
  int right = 2;

  while (
      (left < free_block_count && physical_blocks[left].erase_count <
                                      physical_blocks[center].erase_count) ||
      (right < free_block_count && physical_blocks[right].erase_count <
                                       physical_blocks[center].erase_count)) {
    int min = left;
    if (physical_blocks[right].erase_count < physical_blocks[left].erase_count)
      min = right;

    // swap min and center
    Block *temp = free_blocks[center];
    free_blocks[center] = free_blocks[min];
    free_blocks[min] = temp;

    center = min;
    left = center * 2 + 1;
    right = center * 2 + 2;
  }

  if (free_block_count <=
      (param->physical_page_number / param->page_per_block) *
          param->gc_threshold) {
    gc_flag = true;
  }
  free_block = selected_block;

  return SUCCESS;
}

void MappingTable::add_free_block(const Addr pbn) {
  free_blocks[free_block_count] = &physical_blocks[pbn];

  Addr new_child = free_block_count;
  Addr parent = (new_child - 1) / 2;

  while (new_child != 0 && physical_blocks[parent].erase_count >
                               physical_blocks[new_child].erase_count) {
    Block *temp = free_blocks[parent];
    free_blocks[parent] = free_blocks[new_child];
    free_blocks[new_child] = temp;

    new_child = parent;
    parent = (new_child - 1) / 2;
  }

  free_block_count++;
  map_free_block_count++;
  map_used_block_count--;
}

bool MappingTable::need_gc() {
  return gc_flag;
}

STATE MappingTable::read(const Addr lpn, Addr &ppn) {
  return getppn(lpn, ppn);
}

STATE MappingTable::write(const Addr lpn, Addr &ppn) {
  if (allocate_new_page(lpn, ppn) != SUCCESS) {
    STATE merge_state = merge(lpn);
    if (merge_state == SUCCESS) {
      return allocate_new_page(lpn, ppn);
    }
    else {  // merge is failed
      assert("Failed to write, unsuccessful merge");
      return FAIL;
    }
  }
  return SUCCESS;
}

void MappingTable::PrintStats() {
  // Addr tot_phy_blk_count = param->physical_page_number /
  // param->page_per_block; Addr tot_log_blk_count = param->logical_page_number
  // / param->page_per_block; DPRINTF(FTLOut,  "FTL Map parameters: \n");
  // DPRINTF(FTLOut,  "FTL Map physical block count: %" PRIu64 " \n",
  // tot_phy_blk_count); DPRINTF(FTLOut,  "FTL Map logacal block count: %"
  // PRIu64 " \n", tot_log_blk_count); DPRINTF(FTLOut,  "FTL Map
  // over-provisioning: %.2f \n", param->over_provide); DPRINTF(FTLOut,  "FTL
  // Map GC Strategy: GREEDY \n"); int byte_per_block = (param->page_per_block +
  // 1 + 32 + (int)(log2(param->page_per_block)) + 1) / 8;  // valid bit, bad
  // bit, erase counter, write point, lpn DPRINTF(FTLOut,  "FTL Map Meta data
  // Information for each block: %dB (lpn) + %dB (Valid bits, Bad bit, erase
  // counter, write pointer)\n", param->page_per_block * 8, byte_per_block);
  //
  // DPRINTF(FTLOut,  "FTL Map Total gc count %d \n", map_total_gc_count);
  // DPRINTF(FTLOut,  "FTL Map Total block erase count %d \n",
  // map_block_erase_count); DPRINTF(FTLOut,  "FTL Map GC move count (read,
  // write)  %d  %d \n", map_gc_move_read_count, map_gc_move_write_count);
  // DPRINTF(FTLOut,  "FTL Map Bad block count %d \n", map_bad_block_count); //
  // bad block count
  //
  // if (map_gc_lat_min == DBL_MAX) {
  //   DPRINTF(FTLOut, "FTL Map GC latency (min , max , avg) : ( NA , NA , NA)
  //   us \n");
  // }
  // else {
  //   DPRINTF(FTLOut, "FTL Map GC latency (min , max , avg) : ( %.2f , %.2f ,
  //   %.2f) us \n", map_gc_lat_min, map_gc_lat_max, map_gc_lat_avg);
  // }
  //
  // DPRINTF(FTLOut,  "FTL Map free/used block count: %" PRIu64 "/%" PRIu64
  // "\n", map_free_block_count, map_used_block_count);
}
void MappingTable::ResetStats() {
  map_total_gc_count = 0;
  map_block_erase_count = 0;
  map_bad_block_count = 0;
  map_gc_lat_avg = 0;
  map_gc_lat_min = DBL_MAX;
  map_gc_lat_max = 0;
  map_gc_move_read_count = 0;
  map_gc_move_write_count = 0;
}

void MappingTable::erase_block(int pbn) {
  map_block_erase_count++;
  physical_blocks[pbn].erase_block();

  if (physical_blocks[pbn].erase_count > param->erase_cycle) {
    physical_blocks[pbn].bad_block = true;
    map_bad_block_count++;
  }
  else {  // if the block is NOT a bad block, we don't add it to the free blocks
    add_free_block(pbn);
  }
}
void MappingTable::updateStats(Tick latency) {
  latency = latency / USEC;  // convert ps to us
  map_total_gc_count++;
  if (latency < map_gc_lat_min)
    map_gc_lat_min = latency;
  if (latency > map_gc_lat_max)
    map_gc_lat_max = latency;

  map_gc_lat_avg = (map_gc_lat_avg *
                    ((double)(map_total_gc_count - 1) / map_total_gc_count)) +
                   latency / (double)map_total_gc_count;
}

STATE MappingTable::merge(const Addr lpn) {
  return SUCCESS;
}
