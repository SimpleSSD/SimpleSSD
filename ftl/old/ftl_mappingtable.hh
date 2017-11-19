//
//  MappingTable.h
//  FTL-3
//
//  Created by Narges on 7/4/15.
//  Copyright (c) 2015 narges shahidi. All rights reserved.
//

#ifndef __FTL_3__MappingTable__
#define __FTL_3__MappingTable__

#include <math.h>
#include <cinttypes>
#include <iostream>
#include "ftl_block.hh"
#include "ftl_defs.hh"

class FTL;

class MappingTable {
  friend class FTL;

 protected:
  FTL *ftl;
  Parameter *param;

  Addr free_block_count;
  Block *physical_blocks;
  Block **free_blocks;
  int gc_flag;

  virtual STATE getppn(const Addr lpn, Addr &ppn) = 0;
  virtual STATE find_victim(const Addr *block_list, int count,
                            Addr &victim) = 0;
  virtual STATE merge(const Addr lpn, Tick&);
  virtual STATE allocate_new_page(const Addr lpn, Addr &ppn) = 0;
  virtual STATE find_lpn(const Addr ppn, const Addr group_number,
                         Addr &lpn) = 0;
  STATE get_free_block(Addr &free_block);
  void add_free_block(const Addr pbn);
  void erase_block(int pbn);

 public:
  MappingTable(FTL *);
  ~MappingTable();

  // statistics
  int map_total_gc_count;
  int map_block_erase_count;
  int map_gc_move_read_count;
  int map_gc_move_write_count;
  int map_bad_block_count;
  double map_gc_lat_avg;
  double map_gc_lat_min;
  double map_gc_lat_max;
  Addr map_free_block_count;
  Addr map_used_block_count;

  STATE read(const Addr lpn, Addr &ppn);
  STATE write(const Addr lpn, Addr &ppn, Tick tick);
  virtual Tick GarbageCollection(Tick) = 0;
  bool need_gc();

  virtual void PrintStats();
  virtual void ResetStats();
  virtual void updateStats(Tick latency);
};

#endif /* defined(__FTL_3__MappingTable__) */
