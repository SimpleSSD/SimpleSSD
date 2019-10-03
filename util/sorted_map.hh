// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __UTIL_SORTED_MAP_HH__
#define __UTIL_SORTED_MAP_HH__

#include <functional>
#include <unordered_map>

namespace SimpleSSD {

/**
 * \brief unordered_map + queue
 *
 * Use C Style (no template)
 *
 * Insert item in queue.
 * Access item by key, front or back.
 * Erase item by key, front or back.
 */
class unordered_map_queue {
 protected:
  class Entry {
   public:
    Entry *prev;
    Entry *next;

    uint64_t key;
    void *value;

    Entry();
  };

  uint64_t length;  // Same with map.size()

  std::unordered_map<uint64_t, Entry *> map;

  Entry listHead;
  Entry listTail;

  void eraseMap(uint64_t) noexcept;
  bool insertMap(uint64_t, Entry *) noexcept;
  void eraseList(Entry *) noexcept;
  Entry *insertList(Entry *, void *) noexcept;

 public:
  unordered_map_queue();
  ~unordered_map_queue();

  uint64_t size() noexcept;
  void pop_front() noexcept;
  void pop_back() noexcept;
  bool push_front(uint64_t, void *) noexcept;
  bool push_back(uint64_t, void *) noexcept;
  void *find(uint64_t) noexcept;
  void erase(uint64_t) noexcept;
  void *front() noexcept;
  void *back() noexcept;
};

/**
 * \brief unordered_map + list
 *
 * Use C Style (no template)
 *
 * Insert item in list (w/ provided compare function, O(n)).
 * Access item by key, front or back.
 * Erase item by key, front or back.
 */
class unordered_map_list : public unordered_map_queue {
 public:
  //! Return true if a < b (a should go first)
  using Compare = std::function<bool(const void *, const void *)>;

 protected:
  Compare &func;

  void moveList(Entry *, Entry *) noexcept;

 public:
  unordered_map_list(Compare);
  ~unordered_map_list();

  void pop_front() noexcept = delete;
  void pop_back() noexcept = delete;
  bool push_front(uint64_t, void *) noexcept = delete;
  bool push_back(uint64_t, void *) noexcept = delete;

  bool insert(uint64_t, void *) noexcept;
};

}  // namespace SimpleSSD

#endif
