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
 * \brief map + list
 *
 * Insert item in queue.
 * Access item by key, front or back.
 * Erase item by key, front or back.
 */
template <class Key, class T>
class map_list {
 public:
  using key_type = Key;
  using mapped_type = T;
  using size_type = uint64_t;

 protected:
  using value_type = std::pair<const key_type, mapped_type>;

  class list_item {
   public:
    list_item *prev;
    list_item *next;

    value_type field;

    list_item();
    list_item(const list_item &) = delete;
    list_item(list_item &&rhs) noexcept = delete;
    list_item(value_type &&);

    list_item &operator=(const list_item &) = delete;
    list_item &operator=(list_item &&rhs) = delete;
  };

 public:
  class iterator {
   public:
    using difference_type = int64_t;
    using value_type = std::pair<const key_type, mapped_type>;
    using pointer = value_type *;
    using reference = value_type &;
    using iterator_category = std::bidirectional_iterator_tag;

   private:
    friend map_list;

    const list_item *head;
    const list_item *tail;
    list_item *cur;

    iterator(list_item *, list_item *, list_item *);

   public:
    iterator &operator++();
    iterator &operator--();

    bool operator==(const iterator &rhs);
    bool operator!=(const iterator &rhs);

    reference operator*();
    pointer operator->();
  };

  class const_iterator {
   public:
    using difference_type = int64_t;
    using value_type = const std::pair<const key_type, mapped_type>;
    using pointer = value_type *;
    using reference = value_type &;
    using iterator_category = std::bidirectional_iterator_tag;

   private:
    friend map_list;

    const list_item *head;
    const list_item *tail;
    list_item *cur;

    const_iterator(const list_item *, const list_item *, const list_item *);

   public:
    const_iterator &operator++();
    const_iterator &operator--();
    bool operator==(const const_iterator &rhs) const;
    bool operator!=(const const_iterator &rhs) const;

    reference operator*() const;
    pointer operator->() const;
  };

 protected:
  std::unordered_map<key_type, list_item> map;

  using map_iterator =
      typename std::unordered_map<key_type, list_item>::iterator;

  list_item listHead;
  list_item listTail;

  void eraseMap(const key_type &) noexcept;
  std::pair<map_iterator, bool> insertMap(const key_type &,
                                          value_type &&) noexcept;
  void eraseList(list_item *) noexcept;
  void insertList(list_item *, list_item *) noexcept;

  iterator make_iterator(list_item *) noexcept;
  const_iterator make_iterator(const list_item *) const noexcept;

 public:
  map_list();
  map_list(const map_list &) = delete;
  map_list(map_list &&) noexcept = delete;
  ~map_list();

  map_list &operator=(const map_list &) = delete;
  map_list &operator=(map_list &&) = delete;

  size_type size() noexcept;
  size_type size() const noexcept;

  void reserve(size_t size) { map.reserve(size); }

  void pop_front() noexcept;
  void pop_back() noexcept;

  std::pair<iterator, bool> push_front(const key_type &,
                                       const mapped_type &) noexcept;
  std::pair<iterator, bool> emplace_front(const key_type &,
                                          mapped_type &&) noexcept;
  std::pair<iterator, bool> push_back(const key_type &,
                                      const mapped_type &) noexcept;
  std::pair<iterator, bool> emplace_back(const key_type &,
                                         mapped_type &&) noexcept;

  iterator find(const key_type &) noexcept;
  const_iterator find(const key_type &) const noexcept;

  iterator erase(iterator) noexcept;
  iterator erase(const_iterator) noexcept;

  value_type &front() noexcept;
  const value_type &front() const noexcept;

  value_type &back() noexcept;
  const value_type &back() const noexcept;

  void clear() noexcept;

  iterator begin() noexcept;
  const_iterator begin() const noexcept;

  iterator end() noexcept;
  const_iterator end() const noexcept;
};

/**
 * \brief map + map
 *
 * Insert item in list (w/ provided compare function, O(n)).
 * Access item by key, front or back.
 * Erase item by key, front or back.
 */
template <class Key, class T>
class map_map : public map_list<Key, T> {
 public:
  using key_type = typename map_list<Key, T>::key_type;
  using mapped_type = typename map_list<Key, T>::mapped_type;
  using size_type = typename map_list<Key, T>::size_type;
  using iterator = typename map_list<Key, T>::iterator;

  //! Return true if a < b (a should go first)
  using Compare = std::function<bool(const mapped_type &, const mapped_type &)>;

 protected:
  using list_item = typename map_list<Key, T>::list_item;

  Compare func;

  void moveList(list_item *, list_item *) noexcept;

 public:
  map_map(Compare);
  map_map(const map_map &) = delete;
  map_map(map_map &&) noexcept = delete;
  ~map_map();

  map_map &operator=(const map_map &) = delete;
  map_map &operator=(map_map &&) = delete;

  void pop_front() noexcept = delete;
  void pop_back() noexcept = delete;

  std::pair<iterator, bool> push_front(const key_type &,
                                       const mapped_type &) noexcept = delete;
  std::pair<iterator, bool> emplace_front(const key_type &,
                                          mapped_type &&) noexcept = delete;
  std::pair<iterator, bool> push_back(const key_type &,
                                      const mapped_type &) noexcept = delete;
  std::pair<iterator, bool> emplace_back(const key_type &,
                                         mapped_type &&) noexcept = delete;

  std::pair<iterator, bool> insert(const key_type &,
                                   const mapped_type &) noexcept;
  std::pair<iterator, bool> emplace(const key_type &, mapped_type &&) noexcept;
};

}  // namespace SimpleSSD

#include "util/sorted_map.cc"

#endif
