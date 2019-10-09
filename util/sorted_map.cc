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

map_list::list_item::list_item() : prev(nullptr), next(nullptr) {}

map_list::list_item::list_item(value_type &&value)
    : prev(nullptr), next(nullptr), field(value) {}

map_list::iterator::iterator(list_item *h, list_item *t, list_item *c)
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

map_list::iterator::reference map_list::iterator::operator*() {
  if (cur != head && cur != tail && cur != nullptr) {
    return cur->field;
  }

  return ((list_item *)tail)->field;
}

map_list::iterator::pointer map_list::iterator::operator->() {
  if (cur != head && cur != tail && cur != nullptr) {
    return &cur->field;
  }

  return nullptr;
}

map_list::const_iterator::const_iterator(const list_item *h, const list_item *t,
                                         const list_item *c)
    : head(h), tail(t), cur((list_item *)c) {}

map_list::const_iterator &map_list::const_iterator::operator++() {
  if (cur != nullptr && cur != tail) {
    cur = cur->next;
  }

  return *this;
}

map_list::const_iterator &map_list::const_iterator::operator--() {
  if (cur != nullptr && cur != head->next) {
    cur = cur->prev;
  }

  return *this;
}

bool map_list::const_iterator::operator==(const const_iterator &rhs) const {
  return cur == rhs.cur;
}

bool map_list::const_iterator::operator!=(const const_iterator &rhs) const {
  return cur != rhs.cur;
}

map_list::const_iterator::reference map_list::const_iterator::operator*()
    const {
  if (cur != head && cur != tail && cur != nullptr) {
    return cur->field;
  }

  return tail->field;
}

map_list::const_iterator::pointer map_list::const_iterator::operator->() const {
  if (cur != head && cur != tail && cur != nullptr) {
    return &cur->field;
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

void map_list::eraseMap(const key_type &key) noexcept {
  auto iter = map.find(key);

  if (UNLIKELY(iter != map.end())) {
    map.erase(iter);
  }
}

bool map_list::insertMap(const key_type &key, list_item *value) noexcept {
  auto result = map.emplace(std::make_pair(key, value));

  return result.second;
}

void map_list::eraseList(list_item *entry) noexcept {
  if (entry != &listHead && entry != &listTail) {
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;

    delete entry;
  }
}

map_list::list_item *map_list::insertList(list_item *prev,
                                          value_type &&value) noexcept {
  list_item *entry = new list_item(std::move(value));

  entry->prev = prev;
  entry->next = prev->next;
  prev->next->prev = entry;
  prev->next = entry;

  return entry;
}

map_list::iterator map_list::make_iterator(list_item *entry) noexcept {
  return iterator(&listHead, &listTail, entry);
}

map_list::const_iterator map_list::make_iterator(const list_item *entry) const
    noexcept {
  return const_iterator(&listHead, &listTail, entry);
}

map_list::size_type map_list::size() noexcept {
  return map.size();
}

map_list::size_type map_list::size() const noexcept {
  return map.size();
}

void map_list::pop_front() noexcept {
  if (LIKELY(map.size() > 0)) {
    // Remove first entry from map
    eraseMap(listHead.next->field.first);

    // Remove first entry from list
    eraseList(listHead.next);
  }
}

void map_list::pop_back() noexcept {
  if (LIKELY(map.size() > 0)) {
    // Remove last entry from map
    eraseMap(listTail.prev->field.first);

    // Remove first entry from list
    eraseList(listTail.prev);
  }
}

std::pair<map_list::iterator, bool> map_list::push_front(
    key_type &&key, mapped_type &&value) noexcept {
  if (LIKELY(map.count(key) == 0)) {
    // Insert item to list
    auto entry = insertList(&listHead, std::make_pair(key, value));

    // Insert item to map
    insertMap(key, entry);

    return std::make_pair(iterator(&listHead, &listTail, entry), true);
  }

  return std::make_pair(end(), false);
}

std::pair<map_list::iterator, bool> map_list::push_back(
    key_type &&key, mapped_type &&value) noexcept {
  if (LIKELY(map.count(key) == 0)) {
    // Insert item to list
    auto entry = insertList(listTail.prev, std::make_pair(key, value));

    // Insert item to map
    insertMap(key, entry);

    return std::make_pair(iterator(&listHead, &listTail, entry), true);
  }

  return std::make_pair(end(), false);
}

map_list::iterator map_list::find(const key_type &key) noexcept {
  auto iter = map.find(key);

  if (iter != map.end()) {
    return make_iterator(iter->second);
  }

  return end();
}

map_list::const_iterator map_list::find(const key_type &key) const noexcept {
  auto iter = map.find(key);

  if (iter != map.end()) {
    return make_iterator(iter->second);
  }

  return end();
}

map_list::iterator map_list::erase(iterator iter) noexcept {
  list_item *next = iter.cur->next;

  eraseMap(iter.cur->field.first);
  eraseList(iter.cur);

  return make_iterator(next);
}

map_list::iterator map_list::erase(const_iterator iter) noexcept {
  list_item *next = iter.cur->next;

  eraseMap(iter.cur->field.first);
  eraseList(iter.cur);

  return make_iterator(next);
}

map_list::value_type &map_list::front() noexcept {
  if (LIKELY(map.size() > 0)) {
    return listHead.next->field;
  }

  return listTail.field;
}

const map_list::value_type &map_list::front() const noexcept {
  if (LIKELY(map.size() > 0)) {
    return listHead.next->field;
  }

  return listTail.field;
}

map_list::value_type &map_list::back() noexcept {
  if (LIKELY(map.size() > 0)) {
    return listTail.prev->field;
  }

  return listTail.field;
}

const map_list::value_type &map_list::back() const noexcept {
  if (LIKELY(map.size() > 0)) {
    return listTail.prev->field;
  }

  return listTail.field;
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
  return make_iterator(listHead.next);
}

map_list::iterator map_list::end() noexcept {
  return make_iterator(&listTail);
}

map_list::const_iterator map_list::begin() const noexcept {
  return make_iterator(listHead.next);
}

map_list::const_iterator map_list::end() const noexcept {
  return make_iterator(&listTail);
}

map_map::map_map(Compare c) : map_list(), func(c) {}

map_map::~map_map() {}

std::pair<map_map::iterator, bool> map_map::insert(
    key_type &&key, mapped_type &&value) noexcept {
  if (LIKELY(map.count(key) == 0)) {
    // Find where to insert
    list_item *prev = &listHead;

    while (prev != &listTail) {
      // prev->next->value is nullptr when prev->next is &listTail
      if (prev->next == &listTail || func(value, prev->next->field.second)) {
        break;
      }

      prev = prev->next;
    }

    // Insert item to list
    auto entry = insertList(prev, std::make_pair(key, value));

    // Insert item to map
    insertMap(key, entry);

    return std::make_pair(make_iterator(entry), true);
  }

  return std::make_pair(end(), false);
}

}  // namespace SimpleSSD
