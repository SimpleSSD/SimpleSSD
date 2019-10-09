// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "util/sorted_map.hh"

#include <iostream>

#include "util/algorithm.hh"

namespace SimpleSSD {

map_list::Entry::Entry() : prev(nullptr), next(nullptr) {}

map_list::iterator::iterator(Entry *h, Entry *t, Entry *c)
    : head(h), tail(t), cur(c) {}

map_list::iterator &map_list::iterator::operator++() {
  if (cur != nullptr && cur != tail) {
    cur = cur->next;
  }

  return *this;
}

map_list::iterator &map_list::iterator::operator--() {
  if (cur != nullptr && cur != head->next) {
    cur = cur->prev;
  }

  return *this;
}

bool map_list::iterator::operator==(const iterator &rhs) {
  return cur == rhs.cur;
}

bool map_list::iterator::operator!=(const iterator &rhs) {
  return cur != rhs.cur;
}

uint64_t map_list::iterator::getKey() {
  if (cur != head && cur != tail && cur != nullptr) {
    return cur->key;
  }

  return 0;
}

void *map_list::iterator::getValue() {
  if (cur != head && cur != tail && cur != nullptr) {
    return cur->value;
  }

  return nullptr;
}

map_list::const_iterator::const_iterator(const Entry *h, const Entry *t,
                                         const Entry *c)
    : head(h), tail(t), cur((Entry *)c) {}

map_list::const_iterator map_list::const_iterator::operator++() {
  if (cur != nullptr && cur != tail) {
    cur = cur->next;
  }

  return *this;
}

map_list::const_iterator map_list::const_iterator::operator--() {
  if (cur != nullptr && cur != head->next) {
    cur = cur->prev;
  }

  return *this;
}

bool map_list::const_iterator::operator==(const const_iterator &rhs) {
  return cur == rhs.cur;
}

bool map_list::const_iterator::operator!=(const const_iterator &rhs) {
  return cur != rhs.cur;
}

uint64_t map_list::const_iterator::getKey() const {
  if (cur != head && cur != tail && cur != nullptr) {
    return cur->key;
  }

  return 0;
}

const void *map_list::const_iterator::getValue() const {
  if (cur != head && cur != tail && cur != nullptr) {
    return cur->value;
  }

  return nullptr;
}

map_list::map_list() {
  listHead.next = &listTail;
  listTail.prev = &listHead;
}

map_list::~map_list() {
  clear();
}

void map_list::eraseMap(uint64_t key) noexcept {
  auto iter = map.find(key);

  if (UNLIKELY(iter != map.end())) {
    map.erase(iter);
  }
}

bool map_list::insertMap(uint64_t key, Entry *value) noexcept {
  auto result = map.emplace(std::make_pair(key, value));

  return result.second;
}

void map_list::eraseList(Entry *entry) noexcept {
  if (entry != &listHead && entry != &listTail) {
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;

    delete entry;
  }
}

map_list::Entry *map_list::insertList(Entry *prev, void *value) noexcept {
  Entry *entry = new Entry();

  entry->prev = prev;
  entry->next = prev->next;
  prev->next->prev = entry;
  prev->next = entry;

  entry->value = value;

  return entry;
}

uint64_t map_list::size() noexcept {
  return map.size();
}

void map_list::pop_front() noexcept {
  if (LIKELY(map.size() > 0)) {
    // Remove first entry from map
    eraseMap(listHead.next->key);

    // Remove first entry from list
    eraseList(listHead.next);
  }
}

void map_list::pop_back() noexcept {
  if (LIKELY(map.size() > 0)) {
    // Remove last entry from map
    eraseMap(listTail.prev->key);

    // Remove first entry from list
    eraseList(listTail.prev);
  }
}

bool map_list::push_front(uint64_t key, void *value) noexcept {
  if (LIKELY(map.count(key) == 0)) {
    // Insert item to list
    auto entry = insertList(&listHead, value);

    // Insert item to map
    entry->key = key;
    insertMap(key, entry);

    return true;
  }

  return false;
}

bool map_list::push_back(uint64_t key, void *value) noexcept {
  if (LIKELY(map.count(key) == 0)) {
    // Insert item to list
    auto entry = insertList(listTail.prev, value);

    // Insert item to map
    entry->key = key;
    insertMap(key, entry);

    return true;
  }

  return false;
}

void *map_list::find(uint64_t key) noexcept {
  auto iter = map.find(key);

  if (iter != map.end()) {
    return iter->second->value;
  }

  return nullptr;
}

void map_list::erase(uint64_t key) noexcept {
  auto iter = map.find(key);

  if (iter != map.end()) {
    auto entry = iter->second;

    eraseList(entry);
    map.erase(iter);
  }
}

void *map_list::front() noexcept {
  if (LIKELY(map.size() > 0)) {
    return listHead.next->value;
  }

  return nullptr;
}

void *map_list::back() noexcept {
  if (LIKELY(map.size() > 0)) {
    return listTail.prev->value;
  }

  return nullptr;
}

void map_list::clear() noexcept {
  // Free all objects
  for (auto &iter : map) {
    delete iter.second;
  }

  // Clear structures
  map.clear();

  listHead.next = &listTail;
  listTail.prev = &listHead;
}

map_list::iterator map_list::begin() noexcept {
  return iterator(&listHead, &listTail, listHead.next);
}

map_list::iterator map_list::end() noexcept {
  return iterator(&listHead, &listTail, &listTail);
}

map_list::const_iterator map_list::begin() const noexcept {
  return const_iterator(&listHead, &listTail, listHead.next);
}

map_list::const_iterator map_list::end() const noexcept {
  return const_iterator(&listHead, &listTail, &listTail);
}

uint64_t map_list::size() const noexcept {
  return map.size();
}

map_list::iterator map_list::erase(iterator i) noexcept {
  // Get iterator for internal unordered_map
  auto iter = map.find(i.cur->key);

  if (iter != map.end()) {
    // We need to return next iterator
    Entry *next = i.cur->next;

    erase(i.cur->key);

    return iterator(&listHead, &listTail, next);
  }

  return i;
}

map_map::map_map(Compare c) : map_list(), func(c) {}

map_map::~map_map() {}

bool map_map::insert(uint64_t key, void *value) noexcept {
  if (LIKELY(map.count(key) == 0)) {
    // Find where to insert
    Entry *prev = &listHead;

    while (prev != &listTail) {
      // prev->next->value is nullptr when prev->next is &listTail
      if (prev->next == &listTail || func(value, prev->next->value)) {
        break;
      }

      prev = prev->next;
    }

    // Insert item to list
    auto entry = insertList(prev, value);

    // Insert item to map
    entry->key = key;
    insertMap(key, entry);

    return true;
  }

  return false;
}

}  // namespace SimpleSSD
