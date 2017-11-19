//
//  HybridMapping.cpp
//  FTL-3
//
//  Created by Narges on 7/4/15.
//  Copyright (c) 2015 narges shahidi. All rights reserved.
//

#include "ftl_hybridmapping.hh"

#include "base/trace.hh"
#include "base/types.hh"

/*
 * Data Block Mapping Table (DBMT)
 */
DBMT::DBMT(const Addr block_number) {
  total_block_number = block_number;
  table = new Addr[total_block_number];
  for (Addr i = 0; i < total_block_number; i++) {
    table[i] = Addr(-1);
  }
}
DBMT::~DBMT() {
  // Data Block Mapping Table
  delete[] table;
}
STATE DBMT::get_pbn(const Addr lbn, Addr &pbn) {
  if (lbn >= total_block_number)
    return ERROR;
  pbn = table[lbn];
  if (pbn == -1)
    return FAIL;
  return SUCCESS;
}
STATE DBMT::get_lbn(const Addr pbn, Addr &lbn) {
  for (Addr i = 0; i < total_block_number; i++) {
    if (table[i] == pbn) {
      lbn = i;
      return SUCCESS;
    }
  }
  return FAIL;
}
void DBMT::set_pbn(const Addr lbn, const Addr pbn) {
  table[lbn] = pbn;
}
void DBMT::to_string() {
  for (Addr i = 0; i < total_block_number; i++) {
    if (table[i] != -1)
      std::cout << "(" << i << "->" << table[i] << ")" << std::endl;
  }
}

/*
 * Log Page Mapping Table (LPMT)
 */

LPMT::LPMT(const Addr group_number, int K, int page_per_block) {
  total_group_number = group_number;
  K_Log = K;
  ppb = page_per_block;
  table = new Group *[total_group_number];
  for (int i = 0; i < total_group_number; i++) {
    table[i] = NULL;
  }
}
LPMT::~LPMT() {
  for (Addr i = 0; i < total_group_number; i++) {
    delete table[i];
  }
  delete table;
}
STATE LPMT::lookup(const Addr group_number, const Addr lpn, Addr &ppn) {
  if (group_number >= total_group_number)
    return ERROR;
  if (table[group_number] == NULL)
    return ERROR;
  return table[group_number]->lookup(lpn, ppn);
}
STATE LPMT::lookup_ppn(const Addr group_number, const Addr ppn, Addr &lpn) {
  if (group_number >= total_group_number)
    return ERROR;
  if (table[group_number] == NULL)
    return ERROR;
  return table[group_number]->lookup_ppn(ppn, lpn);
}
STATE LPMT::add(const Addr group_number, const Addr lpn, const Addr ppn) {
  if (group_number >= total_group_number)
    return FAIL;
  if (table[group_number] == NULL) {
    table[group_number] = new Group(group_number, K_Log, ppb);
  }
  return table[group_number]->add_log_page(lpn, ppn);
}
STATE LPMT::remove(const Addr group_number, const Addr lpn) {
  if (group_number >= total_group_number)
    return FAIL;
  if (table[group_number] == NULL)
    return FAIL;
  return table[group_number]->remove_log_page(lpn);
}
void LPMT::to_string() {
  for (Addr i = 0; i < total_group_number; i++) {
    to_string(i);
  }
}
void LPMT::to_string(const Addr group_number) {
  if (table[group_number] != NULL)
    table[group_number]->to_string();
}

LPMT::Group::Group(const Addr gn, const int K, const int ppb) {
  group_number = gn;
  max_log_page = K * ppb;
}
STATE LPMT::Group::add_log_page(const Addr lpn, const Addr ppn) {
  if (lpn == -1) {
    std::cout << "logical page number is invalid! (" << lpn << "," << ppn << ")"
              << std::endl;
    return FAIL;
  }
  group_mtable[lpn] = ppn;
  reverse_mtable[ppn] = lpn;
  return SUCCESS;
}
STATE LPMT::Group::remove_log_page(const Addr lpn) {
  Addr ppn = group_mtable[lpn];
  group_mtable.erase(lpn);
  reverse_mtable.erase(ppn);
  return SUCCESS;
}
STATE LPMT::Group::lookup(const Addr lpn, Addr &ppn) {
  if (group_mtable.find(lpn) == group_mtable.end())
    return FAIL;
  ppn = group_mtable[lpn];
  return SUCCESS;
}
STATE LPMT::Group::lookup_ppn(const Addr ppn, Addr &lpn) {
  if (reverse_mtable.find(ppn) == reverse_mtable.end())
    return FAIL;
  lpn = reverse_mtable[ppn];
  return SUCCESS;
}
void LPMT::Group::to_string() {
  std::cout << "Group[" << group_number << "]: (";
  std::unordered_map<Addr, Addr>::iterator it;
  for (it = group_mtable.begin(); it != group_mtable.end(); it++) {
    std::cout << "(" << it->first << "," << it->second << "),";
  }
  std::cout << endl;
}

/*
 * Log Block Mapping Table (LBMT)
 */

LBMT::LBMT(const Addr group_number, int K) {
  K_Log = K;
  total_group_number = group_number;
  table = new Group *[total_group_number];
  for (Addr i = 0; i < total_group_number; i++) {
    table[i] = NULL;
  }
}
LBMT::~LBMT() {
  for (Addr i = 0; i < total_group_number; i++) {
    delete table[i];
  }
  delete table;
}
Addr *LBMT::getLogBlock(const Addr group_number) {
  if (group_number >= total_group_number)
    return NULL;
  if (table[group_number] == NULL)
    return NULL;
  return table[group_number]->log_blocks;
}
STATE LBMT::getLogBlock(const Addr group_number, int index, Addr &log_block) {
  if (group_number >= total_group_number || index >= K_Log)
    return ERROR;
  if (table[group_number] == NULL)
    return FAIL;
  log_block = table[group_number]->log_blocks[index];
  return SUCCESS;
}
int LBMT::countLogBlock(const Addr group_number) {
  if (group_number >= total_group_number)
    return ERROR;
  if (table[group_number] == NULL)
    return 0;
  return table[group_number]->log_block_number;
}
STATE LBMT::addLogBlock(const Addr group_number, const Addr pbn) {
  if (group_number >= total_group_number)
    return ERROR;
  if (table[group_number] == NULL) {
    Group *g = new Group(group_number, K_Log);
    table[group_number] = g;
  }

  return table[group_number]->add_log_block(pbn);
}
STATE LBMT::removeLogBlock(const Addr group_number, const Addr pbn) {
  if (group_number >= total_group_number)
    return ERROR;
  if (table[group_number] == NULL)
    return ERROR;

  return table[group_number]->remove_log_block(pbn);
}
void LBMT::to_string() {
  for (Addr i = 0; i < total_group_number; i++) {
    if (table[i] != NULL)
      table[i]->to_string();
  }
}
void LBMT::to_string(const Addr group_number) {
  if (table[group_number] != NULL)
    table[group_number]->to_string();
}

LBMT::Group::Group(const Addr gn, int K) {
  log_blocks = new Addr[K];
  group_number = gn;
  for (int i = 0; i < K; i++) {
    log_blocks[i] = -1;
  }
  log_block_number = 0;
  K_Log = K;
}
STATE LBMT::Group::add_log_block(const Addr new_log_block) {
  if (log_block_number >= K_Log)
    return FAIL;
  for (int i = 0; i < K_Log; i++) {
    if (log_blocks[i] == -1) {
      log_blocks[i] = new_log_block;
      log_block_number++;
      return SUCCESS;
    }
  }
  return FAIL;
}
STATE LBMT::Group::remove_log_block(const Addr old_log_block) {
  for (int i = 0; i < K_Log; i++) {
    if (log_blocks[i] == old_log_block) {
      log_blocks[i] = -1;
      log_block_number--;
      return SUCCESS;
    }
  }
  return FAIL;
}
void LBMT::Group::to_string() {
  std::cout << "Group[" << group_number << "]: (";
  for (int j = 0; j < K_Log; j++) {
    std::cout << log_blocks[j] << ",";
  }
  std::cout << ")" << std::endl;
}

/*
 * Hybrid Mapping
 */

HybridMapping::HybridMapping(FTL *f) : MappingTable(f) {
  N_Data = param->mapping_N;
  K_Log = param->mapping_K;

  // Initialize Data Structures
  data_block_MT = new DBMT(param->logical_block_number);

  total_group_number = param->logical_block_number / N_Data;
  if (param->logical_block_number % N_Data != 0)
    total_group_number++;
  log_block_MT = new LBMT(total_group_number, K_Log);
  log_page_MT = new LPMT(total_group_number, K_Log, param->page_per_block);

  ResetStats();
}
HybridMapping::~HybridMapping() {
  delete data_block_MT;
  delete log_block_MT;
  delete log_page_MT;
}
STATE HybridMapping::getppn(const Addr lpn, Addr &ppn) {
  if (lpn >= param->logical_page_number) {
    my_assert("getppn: address out of bound ");
    return FAIL;
  }
  // Lookup in Data Block Table

  Addr lbn = lpn / param->page_per_block;
  Addr page_offset = lpn % param->page_per_block;

  Addr data_pbn;
  if (data_block_MT->get_pbn(lbn, data_pbn) != SUCCESS ||
      data_pbn >= param->physical_page_number / param->page_per_block) {
    my_assert("data block number is not allocated or is invalid ");
    return FAIL;
  }

  if (physical_blocks[data_pbn].get_page_state(page_offset) ==
      Block::PAGE_VALID) {
    ppn = data_pbn * param->page_per_block + page_offset;
    return SUCCESS;
  }

  // Lookup in Log Page Table

  Addr log_ppn;
  Addr group_number = lbn / N_Data;

  if (log_page_MT->lookup(group_number, lpn, log_ppn) != SUCCESS ||
      log_ppn >= param->physical_page_number) {
    return FAIL;
  }

  Addr log_pbn = log_ppn / param->page_per_block;
  Addr log_page_offset = log_ppn % param->page_per_block;

  if (physical_blocks[log_pbn].get_page_state(log_page_offset) ==
      Block::PAGE_VALID) {
    ppn = log_ppn;
    return SUCCESS;
  }

  my_assert("Fail to find physical page number! ");
  return FAIL;
}
STATE HybridMapping::allocate_new_page(const Addr lpn, Addr &ppn) {
  if (lpn > param->logical_page_number) {
    my_assert("allocate_page: address out of bound");
    return ERROR;
  }

  STATE s = insert_into_data_block(lpn, ppn);

  if (s != SUCCESS) {
    return insert_into_log_block(lpn, ppn);
  }

  return s;
}
STATE HybridMapping::insert_into_data_block(const Addr lpn, Addr &ppn) {
  Addr logical_block = lpn / param->page_per_block;
  Addr data_block;
  if (data_block_MT->get_pbn(logical_block, data_block) != SUCCESS) {
    if (get_free_block(data_block) == SUCCESS)
      data_block_MT->set_pbn(logical_block, data_block);
    else
      return FAIL;
  }
  else {
    if (physical_blocks[data_block].get_page_state(
            lpn % param->page_per_block) != Block::PAGE_FREE)
      return FAIL;
  }

  int offset = lpn % param->page_per_block;
  if (physical_blocks[data_block].write_page(lpn, offset) == SUCCESS) {
    ppn = data_block * param->page_per_block + lpn % param->page_per_block;
    return SUCCESS;
  }
  return FAIL;
}
STATE HybridMapping::insert_into_log_block(const Addr lpn, Addr &ppn) {
  Addr group_number = lpn / param->page_per_block / N_Data;

  Addr selected_log_block = -1;
  int i;
  for (i = 0; i < K_Log; i++) {
    if (log_block_MT->getLogBlock(group_number, i, selected_log_block) !=
        SUCCESS)
      continue;
    if (selected_log_block == -1)
      continue;

    if (selected_log_block != -1 &&
        !physical_blocks[selected_log_block].is_full()) {
      break;
    }
  }

  if (i == K_Log) {
    int log_block_count = log_block_MT->countLogBlock(group_number);
    if (log_block_count < K_Log) {
      Addr new_log_block;
      if (get_free_block(new_log_block) != SUCCESS ||
          log_block_MT->addLogBlock(group_number, new_log_block) != SUCCESS) {
        // DPRINTF(FTLOut, "FTL Error in insert_into_log_block \n");
        return ERROR;
      }

      selected_log_block = new_log_block;
    }
    else {
      return FAIL;
    }
  }

  int page_offset =
      -1;  // write on the write_point of block and set page_offset
  if ((selected_log_block == -1) ||
      selected_log_block >
          (param->physical_page_number / param->page_per_block)) {
    // DPRINTF(FTLOut, "FTL Error in insert_into_log_block \n");
    return ERROR;
  }
  if (physical_blocks[selected_log_block].write_page(lpn, page_offset) !=
      SUCCESS) {
    // DPRINTF(FTLOut, "FTL Error couldn't write \n");
    return ERROR;
  }
  invalid_old_page(lpn);
  ppn = selected_log_block * param->page_per_block + page_offset;

  if (log_page_MT->add(group_number, lpn, ppn) != SUCCESS) {
    // DPRINTF(FTLOut, "FTL Error cannot find new entry \n");
    return ERROR;
  }

  return SUCCESS;
}
STATE HybridMapping::invalid_old_page(Addr lpn) {
  Addr lbn = lpn / param->page_per_block;
  Addr page_offset = lpn % param->page_per_block;

  Addr pbn;
  if (data_block_MT->get_pbn(lbn, pbn) != SUCCESS) {
    my_assert("Fail to find pbn for lbn in DBMT");
  }
  if (physical_blocks[pbn].get_page_state(page_offset) == Block::PAGE_VALID) {
    physical_blocks[pbn].set_page_state(page_offset, Block::PAGE_INVALID);
    return SUCCESS;
  }

  // Lookup in Log Page Table
  Addr group_number = lbn / N_Data;
  Addr log_ppn;
  if (log_page_MT->lookup(group_number, lpn, log_ppn) != SUCCESS) {
    return FAIL;
  }
  if (log_page_MT->remove(group_number, lpn) != SUCCESS) {
    my_assert("Fail to remove the entry from LPMT");
    return FAIL;
  }

  Addr log_pbn = log_ppn / param->page_per_block;
  Addr log_page_offset = log_ppn % param->page_per_block;
  if (physical_blocks[log_pbn].get_page_state(log_page_offset) ==
      Block::PAGE_VALID) {
    physical_blocks[log_pbn].set_page_state(log_page_offset,
                                            Block::PAGE_INVALID);
    return SUCCESS;
  }
  // already invalid
  return FAIL;
}

bool HybridMapping::check_direct_erase(const Addr target_block,
                                       const Addr target_group,
                                       const Addr *target_lpns) {
  if (physical_blocks[target_block].valid_page_count() == 0)
    return true;
  return false;
}

bool HybridMapping::check_switch_merge(const Addr target_block,
                                       const Addr target_group,
                                       const Addr *target_lpns) {
  if (target_lpns[0] == -1) {
    return false;
  }

  // the logical block of the first page. The rest of pages should match this
  // one
  Addr logical_block = target_lpns[0] / param->page_per_block;
  if (logical_block / N_Data != target_group) {
    my_assert("Should not happen! a block is saved in wrong group! ");
    return false;
  }

  // for all pages in the target_block, make sure they are valid and has the
  // correct page offset
  for (int i = 0; i < param->page_per_block; i++) {
    if (physical_blocks[target_block].get_page_state(i) != Block::PAGE_VALID) {
      return false;
    }
    Addr saved_lpn = target_lpns[i];
    if (saved_lpn == -1) {
      return false;
    }
    if (saved_lpn % param->page_per_block != i ||
        saved_lpn / param->page_per_block != logical_block) {
      return false;
    }
  }

  return true;
}

bool HybridMapping::check_reorder_merge(const Addr target_block,
                                        const Addr target_group,
                                        const Addr *target_lpns) {
  if (target_lpns[0] == -1) {
    return false;
  }

  Addr logical_block = target_lpns[0] / param->page_per_block;
  if (logical_block / N_Data != target_group) {
    my_assert("Wrong group for logical_block, should not happen!");
    return false;
  }
  for (int i = 0; i < param->page_per_block; ++i) {
    if (physical_blocks[target_block].get_page_state(i) != Block::PAGE_VALID) {
      return false;
    }
    Addr saved_lpn = target_lpns[i];
    if (saved_lpn == -1 ||
        ((saved_lpn / param->page_per_block) != logical_block)) {
      return false;
    }
  }
  return true;
}

bool HybridMapping::check_partial_merge(const Addr target_block,
                                        const Addr target_group,
                                        const Addr *target_lpns) {
  if (target_lpns[0] == -1) {
    return false;
  }

  Addr logical_block = target_lpns[0] / param->page_per_block;
  if (logical_block / N_Data != target_group) {
    my_assert("Fail in partial merge, it shouldn't happen! ");
    return false;
  }
  for (int i = 0; i < param->page_per_block; i++) {
    if (physical_blocks[target_block].get_page_state(i) == Block::PAGE_FREE) {
      break;
    }
    if (physical_blocks[target_block].get_page_state(i) != Block::PAGE_VALID) {
      return false;
    }

    Addr saved_lpn = target_lpns[i];
    if (saved_lpn == -1) {
      return false;
    }
    if (((saved_lpn % param->page_per_block) != i) ||
        ((saved_lpn / param->page_per_block) != logical_block)) {
      return false;
    }
  }
  return true;
}

STATE HybridMapping::do_direct_erase(const Addr target_block,
                                     const Addr target_group,
                                     const Addr *target_lpns, Tick &tick) {
  SimpleSSD::PAL::Request req;

  if (log_block_MT->removeLogBlock(target_group, target_block) != SUCCESS) {
    my_assert("Problem in direct erase: removing from log block list failed! ");
    return FAIL;
  }
  erase_block(target_block);

  req.blockIndex = target_block;

  ftl->eraseInternal(req, tick);
  direct_erase_count++;
  return SUCCESS;
}

STATE HybridMapping::do_switch_merge(const Addr target_block,
                                     const Addr target_group,
                                     const Addr *target_lpns, Tick &tick) {
  SimpleSSD::PAL::Request req;

  if (target_lpns[0] == -1) {
    my_assert("Fail to do switch merge");
    return FAIL;
  }
  Addr logical_block = target_lpns[0] / param->page_per_block;

  Addr current_pbn;
  if (data_block_MT->get_pbn(logical_block, current_pbn) != SUCCESS) {
    my_assert("Switch merge faild to find current pbn in DBMT!");
    return FAIL;
  }

  for (int i = 0; i < param->page_per_block; i++) {
    if (log_page_MT->remove(target_group,
                            logical_block * param->page_per_block + i) !=
        SUCCESS) {
      my_assert("Switch merge failed to remove lpn from the LPMT");
      return FAIL;
    }
  }

  data_block_MT->set_pbn(logical_block, target_block);
  log_block_MT->removeLogBlock(target_group, target_block);
  erase_block(current_pbn);

  req.blockIndex = current_pbn;

  ftl->eraseInternal(req, tick);

  switch_merge_count++;
  return SUCCESS;
}

STATE HybridMapping::do_reorder_merge(const Addr target_block,
                                      const Addr target_group,
                                      const Addr *target_lpns, Tick &tick) {
  SimpleSSD::PAL::Request req;

  if (target_lpns[0]) {
    my_assert("Fail to do reorder merge");
    return FAIL;
  }

  Addr logical_block = target_lpns[0] / param->page_per_block;

  Addr new_pbn;
  if (get_free_block(new_pbn) != SUCCESS) {
    my_assert("Failed in getting a free block! ");
    return FAIL;
  }
  Addr current_pbn;
  if (data_block_MT->get_pbn(logical_block, current_pbn) != SUCCESS) {
    my_assert("Failed in getting the current pbn!");
    return FAIL;
  }

  for (int i = 0; i < param->page_per_block; i++) {
    Addr new_lpn = logical_block * param->page_per_block + i;
    Addr read_ppn;
    if (getppn(new_lpn, read_ppn) == SUCCESS) {
      req.blockIndex = read_ppn / param->page_per_block;
      req.pageIndex = read_ppn % param->page_per_block;

      ftl->readInternal(req, tick, true);
      map_gc_move_read_count++;
    }
    invalid_old_page(new_lpn);  // invalidate and remove from lpmt if exists

    if (physical_blocks[new_pbn].write_page(new_lpn, i) != SUCCESS) {
      my_assert("Fail to write block! ");
      return FAIL;
    }

    req.blockIndex = new_pbn;
    req.pageIndex = i;

    ftl->writeInternal(req, tick, true);
    map_gc_move_write_count++;
  }
  if (log_block_MT->removeLogBlock(target_group, target_block) != SUCCESS) {
    my_assert("Fail to remove log block");
  }
  data_block_MT->set_pbn(logical_block, new_pbn);

  erase_block(current_pbn);
  erase_block(target_block);

  req.blockIndex = current_pbn;
  req.pageIndex = 0;

  ftl->eraseInternal(req, tick);

  req.blockIndex = target_block;

  ftl->eraseInternal(req, tick);

  reorder_merge_count++;
  return SUCCESS;
}

STATE HybridMapping::do_partial_merge(const Addr target_block,
                                      const Addr target_group,
                                      const Addr *target_lpns, Tick &tick) {
  SimpleSSD::PAL::Request req;

  if (target_lpns[0] == -1) {
    my_assert("Fail to do partial merge");
    return FAIL;
  }
  Addr logical_block = target_lpns[0] / param->page_per_block;

  Addr current_pbn;
  if (data_block_MT->get_pbn(logical_block, current_pbn) != SUCCESS) {
    my_assert("Fail in finding the current pbn for a logical block ");
    return FAIL;
  }
  for (int i = 0; i < physical_blocks[target_block].page_sequence_number; i++) {
    Addr new_lpn = logical_block * param->page_per_block + i;
    if (log_page_MT->remove(target_group, new_lpn) != SUCCESS) {
      my_assert("Fail to remove page from LPMT");
    }
  }

  for (int i = physical_blocks[target_block].page_sequence_number;
       i < param->page_per_block; i++) {
    Addr new_lpn = logical_block * param->page_per_block + i;
    Addr read_ppn;
    if (getppn(new_lpn, read_ppn) == SUCCESS) {
      req.blockIndex = read_ppn / param->page_per_block;
      req.pageIndex = read_ppn % param->page_per_block;

      ftl->readInternal(req, tick, true);
      map_gc_move_read_count++;
    }
    invalid_old_page(new_lpn);

    int temp_i = i;
    if (physical_blocks[target_block].write_page(new_lpn, temp_i) == SUCCESS) {
      req.blockIndex = target_block;
      req.pageIndex = i;
      ftl->writeInternal(req, tick, true);
      map_gc_move_write_count++;
    }
  }
  if (log_block_MT->removeLogBlock(target_group, target_block) != SUCCESS) {
    my_assert("Fail to remove log block");
    return FAIL;
  }
  data_block_MT->set_pbn(logical_block, target_block);
  erase_block(current_pbn);
  req.blockIndex = current_pbn;
  req.pageIndex = 0;
  ftl->eraseInternal(req, tick);
  partial_merge_count++;
  return SUCCESS;
}

STATE HybridMapping::do_full_merge(const Addr target_block,
                                   const Addr target_group,
                                   const Addr *target_lpns, Tick &tick) {
  SimpleSSD::PAL::Request req;

  for (int i = 0; i < param->page_per_block; i++) {
    if (physical_blocks[target_block].get_page_state(i) != Block::PAGE_VALID)
      continue;
    if (target_lpns[i] == -1) {
      continue;
    }
    Addr target_lbn = target_lpns[i] / param->page_per_block;

    Addr free_block;
    if (get_free_block(free_block) != SUCCESS) {
      my_assert("Fail to receive a free block");
      return FAIL;
    }

    // Collect all pages of target_lbn in the free block
    for (int j = 0; j < param->page_per_block; j++) {
      Addr copy_lpn = target_lbn * param->page_per_block + j;
      Addr copy_ppn;
      if (getppn(copy_lpn, copy_ppn) == SUCCESS) {
        req.blockIndex = copy_ppn / param->page_per_block;
        req.pageIndex = copy_ppn % param->page_per_block;
        ftl->readInternal(req, tick, true);
        map_gc_move_read_count++;
      }

      int temp_j = j;
      if (physical_blocks[free_block].write_page(copy_lpn, temp_j) != SUCCESS) {
        my_assert("problem in writing to free block ");
        return FAIL;
      }

      invalid_old_page(copy_lpn);  // invalid the page in physical blocks,
                                   // remove from lpmt if it's there
      req.blockIndex = free_block;
      req.pageIndex = j;

      ftl->writeInternal(req, tick, true);
      map_gc_move_write_count++;
    }

    Addr current_pbn = -1;
    if (data_block_MT->get_pbn(target_lbn, current_pbn) != SUCCESS) {
      my_assert("Fail to find the current pbn for a logical block");
    }
    erase_block(current_pbn);

    req.blockIndex = current_pbn;
    req.pageIndex = 0;

    ftl->eraseInternal(req, tick);
    data_block_MT->set_pbn(target_lbn, free_block);
  }

  if (log_block_MT->removeLogBlock(target_group, target_block) != SUCCESS) {
    my_assert("Fail to remove log block from LBMT");
  }

  erase_block(target_block);

  req.blockIndex = target_block;
  req.pageIndex = 0;

  ftl->eraseInternal(req, tick);
  full_merge_count++;

  return SUCCESS;
}

STATE HybridMapping::merge(const Addr lpn, Tick &tick) {
  MappingTable::merge(lpn, tick);

  const Addr group_number = lpn / param->page_per_block / N_Data;
  Addr merge_target;
  if ((find_victim(log_block_MT->getLogBlock(group_number), K_Log,
                   merge_target) != SUCCESS) ||
      (merge_target == -1) ||
      (merge_target >= (param->physical_page_number / param->page_per_block))) {
    my_assert("problem in find victim function ");
    return FAIL;
  }

  Addr *target_lpns = new Addr[param->page_per_block];

  for (Addr i = 0; i < param->page_per_block; i++) {
    if (find_lpn(merge_target * param->page_per_block + i, group_number,
                 target_lpns[i]) != SUCCESS) {
      target_lpns[i] = -1;
    }
  }

  STATE out_state = FAIL;
  /*
   * check direct erase
   */
  if (check_direct_erase(merge_target, group_number, target_lpns)) {
    out_state = do_direct_erase(merge_target, group_number, target_lpns, tick);
    delete target_lpns;
    return out_state;
  }

  /*
   * check switch merge
   */
  if (check_switch_merge(merge_target, group_number, target_lpns)) {
    out_state = do_switch_merge(merge_target, group_number, target_lpns, tick);
    delete target_lpns;
    return out_state;
  }

  /*
   * check reorder merge
   */
  if (check_reorder_merge(merge_target, group_number, target_lpns)) {
    out_state = do_reorder_merge(merge_target, group_number, target_lpns, tick);
    delete target_lpns;
    return out_state;
  }

  /*
   * check for partial merge
   */

  if (check_partial_merge(merge_target, group_number, target_lpns)) {
    out_state = do_partial_merge(merge_target, group_number, target_lpns, tick);
    delete target_lpns;
    return out_state;
  }

  /*
   * do full merge
   */

  out_state = do_full_merge(merge_target, group_number, target_lpns, tick);
  delete target_lpns;
  return out_state;
}
STATE HybridMapping::find_lpn(const Addr ppn, const Addr target_group,
                              Addr &lpn) {
  Addr pbn = ppn / param->page_per_block;
  Addr page_offset = ppn % param->page_per_block;

  Addr lbn;
  if (data_block_MT->get_lbn(pbn, lbn) == SUCCESS) {
    if (physical_blocks[pbn].get_page_state(page_offset) == Block::PAGE_VALID) {
      lpn = lbn * param->page_per_block + page_offset;
      return SUCCESS;
    }
  }

  if (log_page_MT->lookup_ppn(target_group, ppn, lpn) == SUCCESS)
    return SUCCESS;

  return FAIL;
}

STATE HybridMapping::find_victim(const Addr *block_list, int count,
                                 Addr &victim) {
  // Greedy
  if (count == 0)
    return FAIL;
  victim = -1;
  int valid_count = param->page_per_block + 1;
  for (int i = 0; i < count; i++) {
    if (block_list[i] == -1)
      continue;
    if (physical_blocks[block_list[i]].free_page_count() != 0)
      continue;
    int temp_count = physical_blocks[block_list[i]].valid_page_count();
    if (temp_count < valid_count) {
      valid_count = temp_count;
      victim = block_list[i];
    }
  }

  if (victim == -1) {
    victim = block_list[0];
    int valid_count = param->page_per_block + 1;
    for (int i = 0; i < count; i++) {
      if (block_list[i] == -1)
        continue;
      int temp_count = physical_blocks[block_list[i]].valid_page_count();
      if (temp_count < valid_count) {
        valid_count = temp_count;
        victim = block_list[i];
      }
    }
  }
  return SUCCESS;
}
void HybridMapping::map_to_string() {
  log_page_MT->to_string();
  log_block_MT->to_string();
}
void HybridMapping::map_to_string(const Addr group_number) {
  log_page_MT->to_string(group_number);
  log_block_MT->to_string(group_number);
}
Tick HybridMapping::GarbageCollection(Tick tick) {
  global_gc_count++;

  STATE state = FAIL;
  while (state != SUCCESS) {
    Addr selected_group_number = -1;

    int max = 0;
    for (int i = 0; i < total_group_number; i++) {
      int log_block_number = log_block_MT->countLogBlock(i);

      if (log_block_number > max) {
        max = log_block_number;
        selected_group_number = i;
      }
    }
    if (max > 0) {
      state = merge(selected_group_number * N_Data * param->page_per_block, tick);
    }
    else {
      gc_flag = false;
      break;
    }
  }
  gc_flag = false;

  return tick;
}

void HybridMapping::PrintStats() {
  MappingTable::PrintStats();
}
void HybridMapping::ResetStats() {
  direct_erase_count = 0;
  partial_merge_count = 0;
  reorder_merge_count = 0;
  switch_merge_count = 0;
  full_merge_count = 0;
  global_gc_count = 0;

  MappingTable::ResetStats();
}
