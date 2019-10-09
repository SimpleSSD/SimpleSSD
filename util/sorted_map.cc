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

unordered_map_queue::Entry::Entry() : prev(nullptr), next(nullptr) {}

unordered_map_queue::iterator::iterator(Entry *h, Entry *t, Entry *c)
    : head(h), tail(t), cur(c) {}

unordered_map_queue::iterator &unordered_map_queue::iterator::operator++() {
  if (cur != nullptr && cur != tail) {
    cur = cur->next;
  }

  return *this;
}

unordered_map_queue::iterator &unordered_map_queue::iterator::operator--() {
  if (cur != nullptr && cur != head->next) {
    cur = cur->prev;
  }

  return *this;
}

bool unordered_map_queue::iterator::operator==(const iterator &rhs) {
  return cur == rhs.cur;
}

bool unordered_map_queue::iterator::operator!=(const iterator &rhs) {
  return cur != rhs.cur;
}

uint64_t unordered_map_queue::iterator::getKey() {
  if (cur != head && cur != tail && cur != nullptr) {
    return cur->key;
  }

  return 0;
}

void *unordered_map_queue::iterator::getValue() {
  if (cur != head && cur != tail && cur != nullptr) {
    return cur->value;
  }

  return nullptr;
}

unordered_map_queue::const_iterator::const_iterator(const Entry *h,
                                                    const Entry *t,
                                                    const Entry *c)
    : head(h), tail(t), cur((Entry *)c) {}

unordered_map_queue::const_iterator
unordered_map_queue::const_iterator::operator++() {
  if (cur != nullptr && cur != tail) {
    cur = cur->next;
  }

  return *this;
}

unordered_map_queue::const_iterator
unordered_map_queue::const_iterator::operator--() {
  if (cur != nullptr && cur != head->next) {
    cur = cur->prev;
  }

  return *this;
}

bool unordered_map_queue::const_iterator::operator==(
    const const_iterator &rhs) {
  return cur == rhs.cur;
}

bool unordered_map_queue::const_iterator::operator!=(
    const const_iterator &rhs) {
  return cur != rhs.cur;
}

uint64_t unordered_map_queue::const_iterator::getKey() const {
  if (cur != head && cur != tail && cur != nullptr) {
    return cur->key;
  }

  return 0;
}

const void *unordered_map_queue::const_iterator::getValue() const {
  if (cur != head && cur != tail && cur != nullptr) {
    return cur->value;
  }

  return nullptr;
}

unordered_map_queue::unordered_map_queue() {
  listHead.next = &listTail;
  listTail.prev = &listHead;
}

unordered_map_queue::~unordered_map_queue() {
  clear();
}

void unordered_map_queue::eraseMap(uint64_t key) noexcept {
  auto iter = map.find(key);

  if (UNLIKELY(iter != map.end())) {
    map.erase(iter);
  }
}

bool unordered_map_queue::insertMap(uint64_t key, Entry *value) noexcept {
  auto result = map.emplace(std::make_pair(key, value));

  return result.second;
}

void unordered_map_queue::eraseList(Entry *entry) noexcept {
  if (entry != &listHead && entry != &listTail) {
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;

    free(entry);
  }
}

unordered_map_queue::Entry *unordered_map_queue::insertList(
    Entry *prev, void *value) noexcept {
  Entry *entry = new Entry();

  entry->prev = prev;
  entry->next = prev->next;
  prev->next->prev = entry;
  prev->next = entry;

  entry->value = value;

  return entry;
}

uint64_t unordered_map_queue::size() noexcept {
  return map.size();
}

void unordered_map_queue::pop_front() noexcept {
  if (LIKELY(map.size() > 0)) {
    // Remove first entry from map
    eraseMap(listHead.next->key);

    // Remove first entry from list
    eraseList(listHead.next);
  }
}

void unordered_map_queue::pop_back() noexcept {
  if (LIKELY(map.size() > 0)) {
    // Remove last entry from map
    eraseMap(listTail.prev->key);

    // Remove first entry from list
    eraseList(listTail.prev);
  }
}

bool unordered_map_queue::push_front(uint64_t key, void *value) noexcept {
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

bool unordered_map_queue::push_back(uint64_t key, void *value) noexcept {
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

void *unordered_map_queue::find(uint64_t key) noexcept {
  auto iter = map.find(key);

  if (iter != map.end()) {
    return iter->second->value;
  }

  return nullptr;
}

void unordered_map_queue::erase(uint64_t key) noexcept {
  auto iter = map.find(key);

  if (iter != map.end()) {
    auto entry = iter->second;

    eraseList(entry);
    map.erase(iter);
  }
}

void *unordered_map_queue::front() noexcept {
  if (LIKELY(map.size() > 0)) {
    return listHead.next->value;
  }

  return nullptr;
}

void *unordered_map_queue::back() noexcept {
  if (LIKELY(map.size() > 0)) {
    return listTail.prev->value;
  }

  return nullptr;
}

void unordered_map_queue::clear() noexcept {
  // Free all objects
  for (auto &iter : map) {
    free(iter.second);
  }

  // Clear structures
  map.clear();

  listHead.next = &listTail;
  listTail.prev = &listHead;
}

unordered_map_queue::iterator unordered_map_queue::begin() noexcept {
  return iterator(&listHead, &listTail, listHead.next);
}

unordered_map_queue::iterator unordered_map_queue::end() noexcept {
  return iterator(&listHead, &listTail, &listTail);
}

unordered_map_queue::const_iterator unordered_map_queue::begin() const
    noexcept {
  return const_iterator(&listHead, &listTail, listHead.next);
}

unordered_map_queue::const_iterator unordered_map_queue::end() const noexcept {
  return const_iterator(&listHead, &listTail, &listTail);
}

uint64_t unordered_map_queue::size() const noexcept {
  return map.size();
}

unordered_map_queue::iterator unordered_map_queue::erase(iterator &i) noexcept {
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

unordered_map_list::unordered_map_list(Compare c)
    : unordered_map_queue(), func(c) {}

unordered_map_list::~unordered_map_list() {}

bool unordered_map_list::insert(uint64_t key, void *value) noexcept {
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
