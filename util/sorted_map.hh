// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_UTIL_SORTED_MAP_HH__
#define __SIMPLESSD_UTIL_SORTED_MAP_HH__

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

 public:
  class iterator {
   private:
    friend unordered_map_queue;

    const Entry *head;
    const Entry *tail;
    Entry *cur;

    iterator(Entry *, Entry *, Entry *);

   public:
    iterator &operator++();
    iterator &operator--();
    bool operator==(const iterator &rhs);
    bool operator!=(const iterator &rhs);

    uint64_t getKey();
    void *getValue();
  };

  class const_iterator {
   private:
    friend unordered_map_queue;

    const Entry *head;
    const Entry *tail;
    Entry *cur;  // Not const because I have to incr/decr pointer

    const_iterator(const Entry *, const Entry *, const Entry *);

   public:
    const_iterator operator++();
    const_iterator operator--();
    bool operator==(const const_iterator &rhs);
    bool operator!=(const const_iterator &rhs);

    uint64_t getKey() const;
    const void *getValue() const;
  };

 protected:
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
  void clear() noexcept;

  iterator begin() noexcept;
  iterator end() noexcept;
  iterator erase(iterator &) noexcept;

  uint64_t size() const noexcept;
  const_iterator begin() const noexcept;
  const_iterator end() const noexcept;
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
