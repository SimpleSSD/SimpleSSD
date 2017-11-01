//
//  HybridMapping.h
//  FTL-3
//
//  Created by Narges on 7/4/15.
//  Copyright (c) 2015 narges shahidi. All rights reserved.
//

#ifndef __FTL_3__HybridMapping__
#define __FTL_3__HybridMapping__

#include <iostream>
#include <unordered_map>
#include "ftl_mappingtable.hh"
#include "ftl.hh"

class DBMT{
    Addr total_block_number;
    Addr * table;      // DBMT: Data Block Mapping Table (maps lbn to pbn)

  public:
    DBMT(const Addr block_number);
    ~DBMT();

    STATE get_pbn(const Addr lbn, Addr & pbn);
    STATE get_lbn(const Addr pbn, Addr & lbn);
    void set_pbn(const Addr lbn, const Addr pbn);
    void to_string();
};

class LPMT{
  private:
    class Group{
      public:
        Addr group_number;
        Addr max_log_page;
        std::unordered_map<Addr, Addr> group_mtable;
        std::unordered_map<Addr, Addr> reverse_mtable;
        Group(const Addr gn, const int K, const int ppb);
        STATE add_log_page(const Addr lpn, const Addr ppn);
        STATE remove_log_page(const Addr lpn);
        STATE lookup(const Addr lpn, Addr &ppn);
        STATE lookup_ppn(const Addr ppn, Addr &lpn);
        void to_string();
    };

    Addr total_group_number;
    int K_Log;
    int ppb;
    Group ** table;      // LPMT: Log Page Mapping Table

  public:
    LPMT(Addr group_number, int K, int page_per_block);
    ~LPMT();

    STATE lookup(const Addr group_number, const Addr lpn, Addr & ppn);
    STATE lookup_ppn(const Addr group_number, const Addr ppn, Addr & lpn);
    STATE add(const Addr group_number, const Addr lpn, const Addr ppn);
    STATE remove(const Addr group_number, const Addr lpn);
    void to_string();
    void to_string(const Addr group_number);
};

class LBMT{
  private:
    class Group{
      public:
        Addr group_number;
        Addr * log_blocks;
        int log_block_number;  // Less than or equal to K_log
        int K_Log;

        Group(Addr gn, int K);
        STATE add_log_block(const Addr new_log_block);
        STATE remove_log_block(const Addr old_log_block);
        void to_string();
    };

    int K_Log;
    Addr total_group_number;

    Group ** table;

  public:
    LBMT(const Addr group_number, int K);
    ~LBMT();
    Addr * getLogBlock(const Addr group_number);
    STATE getLogBlock(const Addr group_number, int index, Addr & log_block);
    int countLogBlock(Addr group_number);
    STATE addLogBlock(Addr group_number, Addr pbn);
    STATE removeLogBlock(Addr group_number, Addr pbn);
    void to_string();
    void to_string(const Addr group_number);
};

class HybridMapping : virtual public MappingTable {
  private:
    DBMT * data_block_MT;
    LBMT * log_block_MT;
    LPMT * log_page_MT;

    int N_Data;
    int K_Log;
    Addr total_group_number;

    STATE insert_into_data_block(const Addr lpn, Addr &ppn);
    STATE insert_into_log_block(const Addr lpn, Addr &ppn);
    void update_log_page_MT(const Addr lpn, const Addr ppn, int operation );
    STATE find_lpn(const Addr ppn, const Addr group_number, Addr &lpn);
    STATE invalid_old_page(const Addr lpn);

    // super class function
    STATE find_victim(const Addr * block_list, int count, Addr & victim);
    STATE getppn(const Addr lpn, Addr & ppn);
    STATE merge(const Addr lpn);
    STATE allocate_new_page(const Addr lpn, Addr & ppn);

    // merge functions
    bool check_direct_erase  (const Addr target_block, const Addr target_group, const Addr * target_lpns);
    bool check_switch_merge  (const Addr target_block, const Addr target_group, const Addr * target_lpns);
    bool check_reorder_merge (const Addr target_block, const Addr target_group, const Addr * target_lpns);
    bool check_partial_merge (const Addr target_block, const Addr target_group, const Addr * target_lpns);
    bool check_full_merge    (const Addr target_block, const Addr target_group, const Addr * target_lpns);

    STATE do_direct_erase    (const Addr target_block, const Addr target_group, const Addr * target_lpns);
    STATE do_switch_merge    (const Addr target_block, const Addr target_group, const Addr * target_lpns);
    STATE do_reorder_merge   (const Addr target_block, const Addr target_group, const Addr * target_lpns);
    STATE do_partial_merge   (const Addr target_block, const Addr target_group, const Addr * target_lpns);
    STATE do_full_merge      (const Addr target_block, const Addr target_group, const Addr * target_lpns);

  public:
    HybridMapping(FTL *);
    ~HybridMapping();

    // Statistics
    int direct_erase_count;
    int partial_merge_count;
    int reorder_merge_count;
    int switch_merge_count;
    int full_merge_count;
    int global_gc_count;

    Tick lastGCTick;

    // super class functions

    Tick GarbageCollection();

    void PrintStats();
    void ResetStats();

    // to_string functions
    void map_to_string();
    void map_to_string(const Addr group_number);
};

#endif /* defined(__FTL_3__HybridMapping__) */
