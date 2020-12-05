// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#ifndef SORTED_MAP_TEMPLATE
#define SORTED_MAP_TEMPLATE                                                    \
  template <class Key, class T, std::enable_if_t<std::is_pointer_v<T>, bool> U>

#include "util/sorted_map.hh"

#include "util/algorithm.hh"

namespace SimpleSSD {

SORTED_MAP_TEMPLATE
map_list<Key, T, U>::list_item::list_item() : prev(nullptr), next(nullptr) {}

SORTED_MAP_TEMPLATE
map_list<Key, T, U>::list_item::list_item(value_type &&value)
    : prev(nullptr), next(nullptr), field(value) {}

SORTED_MAP_TEMPLATE
map_list<Key, T, U>::iterator::iterator(list_item *h, list_item *t,
                                        list_item *c)
    : head(h), tail(t), cur(c) {}

SORTED_MAP_TEMPLATE
typename map_list<Key, T, U>::iterator &
map_list<Key, T, U>::iterator::operator++() {
  if (cur != nullptr && cur != tail) {
    cur = cur->next;
  }

  return *this;
}

SORTED_MAP_TEMPLATE
typename map_list<Key, T, U>::iterator &
map_list<Key, T, U>::iterator::operator--() {
  if (cur != nullptr && cur != head->next) {
    cur = cur->prev;
  }

  return *this;
}

SORTED_MAP_TEMPLATE
bool map_list<Key, T, U>::iterator::operator==(const iterator &rhs) {
  return cur == rhs.cur;
}

SORTED_MAP_TEMPLATE
bool map_list<Key, T, U>::iterator::operator!=(const iterator &rhs) {
  return cur != rhs.cur;
}

SORTED_MAP_TEMPLATE
typename map_list<Key, T, U>::iterator::reference
map_list<Key, T, U>::iterator::operator*() {
  if (cur != head && cur != tail && cur != nullptr) {
    return cur->field;
  }

  return ((list_item *)tail)->field;
}

SORTED_MAP_TEMPLATE
typename map_list<Key, T, U>::iterator::pointer
map_list<Key, T, U>::iterator::operator->() {
  if (cur != head && cur != tail && cur != nullptr) {
    return &cur->field;
  }

  return nullptr;
}

SORTED_MAP_TEMPLATE
map_list<Key, T, U>::const_iterator::const_iterator(const list_item *h,
                                                    const list_item *t,
                                                    const list_item *c)
    : head(h), tail(t), cur((list_item *)c) {}

SORTED_MAP_TEMPLATE
typename map_list<Key, T, U>::const_iterator &
map_list<Key, T, U>::const_iterator::operator++() {
  if (cur != nullptr && cur != tail) {
    cur = cur->next;
  }

  return *this;
}

SORTED_MAP_TEMPLATE
typename map_list<Key, T, U>::const_iterator &
map_list<Key, T, U>::const_iterator::operator--() {
  if (cur != nullptr && cur != head->next) {
    cur = cur->prev;
  }

  return *this;
}

SORTED_MAP_TEMPLATE
bool map_list<Key, T, U>::const_iterator::operator==(
    const const_iterator &rhs) const {
  return cur == rhs.cur;
}

SORTED_MAP_TEMPLATE
bool map_list<Key, T, U>::const_iterator::operator!=(
    const const_iterator &rhs) const {
  return cur != rhs.cur;
}

SORTED_MAP_TEMPLATE
typename map_list<Key, T, U>::const_iterator::reference
map_list<Key, T, U>::const_iterator::operator*() const {
  if (cur != head && cur != tail && cur != nullptr) {
    return cur->field;
  }

  return tail->field;
}

SORTED_MAP_TEMPLATE
typename map_list<Key, T, U>::const_iterator::pointer
map_list<Key, T, U>::const_iterator::operator->() const {
  if (cur != head && cur != tail && cur != nullptr) {
    return &cur->field;
  }

  return nullptr;
}

SORTED_MAP_TEMPLATE
map_list<Key, T, U>::map_list() {
  listHead = new list_item();
  listTail = new list_item();

  listHead->next = listTail;
  listTail->prev = listHead;
}

SORTED_MAP_TEMPLATE
map_list<Key, T, U>::map_list(map_list &&rhs) noexcept {
  *this = std::move(rhs);
}

SORTED_MAP_TEMPLATE
map_list<Key, T, U>::~map_list() {
  clear();

  delete listHead;
  delete listTail;
}

SORTED_MAP_TEMPLATE
map_list<Key, T, U> &map_list<Key, T, U>::operator=(map_list &&rhs) {
  if (this != &rhs) {
    map = std::move(rhs.map);
    listHead = std::exchange(rhs.listHead, nullptr);
    listTail = std::exchange(rhs.listTail, nullptr);
  }

  return *this;
}

SORTED_MAP_TEMPLATE
void map_list<Key, T, U>::eraseMap(const key_type &key) noexcept {
  auto iter = map.find(key);

  if (UNLIKELY(iter != map.end())) {
    map.erase(iter);
  }
}

SORTED_MAP_TEMPLATE
bool map_list<Key, T, U>::insertMap(const key_type &key,
                                    list_item *value) noexcept {
  auto result = map.emplace(key, value);

  return result.second;
}

SORTED_MAP_TEMPLATE
void map_list<Key, T, U>::eraseList(list_item *entry) noexcept {
  if (entry != listHead && entry != listTail) {
    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;

    delete entry;
  }
}

SORTED_MAP_TEMPLATE
typename map_list<Key, T, U>::list_item *map_list<Key, T, U>::insertList(
    list_item *prev, value_type &&value) noexcept {
  list_item *entry = new list_item(std::move(value));

  entry->prev = prev;
  entry->next = prev->next;
  prev->next->prev = entry;
  prev->next = entry;

  return entry;
}

SORTED_MAP_TEMPLATE
typename map_list<Key, T, U>::iterator map_list<Key, T, U>::make_iterator(
    list_item *entry) noexcept {
  return iterator(listHead, listTail, entry);
}

SORTED_MAP_TEMPLATE
typename map_list<Key, T, U>::const_iterator map_list<Key, T, U>::make_iterator(
    const list_item *entry) const noexcept {
  return const_iterator(listHead, listTail, entry);
}

SORTED_MAP_TEMPLATE
typename map_list<Key, T, U>::size_type map_list<Key, T, U>::size() noexcept {
  return map.size();
}

SORTED_MAP_TEMPLATE
typename map_list<Key, T, U>::size_type map_list<Key, T, U>::size()
    const noexcept {
  return map.size();
}

SORTED_MAP_TEMPLATE
void map_list<Key, T, U>::pop_front() noexcept {
  if (LIKELY(map.size() > 0)) {
    // Remove first entry from map
    eraseMap(listHead->next->field.first);

    // Remove first entry from list
    eraseList(listHead->next);
  }
}

SORTED_MAP_TEMPLATE
void map_list<Key, T, U>::pop_back() noexcept {
  if (LIKELY(map.size() > 0)) {
    // Remove last entry from map
    eraseMap(listTail->prev->field.first);

    // Remove first entry from list
    eraseList(listTail->prev);
  }
}

SORTED_MAP_TEMPLATE
std::pair<typename map_list<Key, T, U>::iterator, bool>
map_list<Key, T, U>::push_front(const key_type &key,
                                const mapped_type &value) noexcept {
  if (LIKELY(map.count(key) == 0)) {
    // Insert item to list
    auto entry = insertList(listHead, std::make_pair(key, value));

    // Insert item to map
    insertMap(key, entry);

    return std::make_pair(make_iterator(entry), true);
  }

  return std::make_pair(end(), false);
}

SORTED_MAP_TEMPLATE
std::pair<typename map_list<Key, T, U>::iterator, bool>
map_list<Key, T, U>::push_back(const key_type &key,
                               const mapped_type &value) noexcept {
  if (LIKELY(map.count(key) == 0)) {
    // Insert item to list
    auto entry = insertList(listTail->prev, std::make_pair(key, value));

    // Insert item to map
    insertMap(key, entry);

    return std::make_pair(make_iterator(entry), true);
  }

  return std::make_pair(end(), false);
}

SORTED_MAP_TEMPLATE
typename map_list<Key, T, U>::iterator map_list<Key, T, U>::find(
    const key_type &key) noexcept {
  auto iter = map.find(key);

  if (iter != map.end()) {
    return make_iterator(iter->second);
  }

  return end();
}

SORTED_MAP_TEMPLATE
typename map_list<Key, T, U>::const_iterator map_list<Key, T, U>::find(
    const key_type &key) const noexcept {
  auto iter = map.find(key);

  if (iter != map.end()) {
    return make_iterator(iter->second);
  }

  return end();
}

SORTED_MAP_TEMPLATE
typename map_list<Key, T, U>::iterator map_list<Key, T, U>::erase(
    iterator iter) noexcept {
  list_item *next = iter.cur->next;

  eraseMap(iter.cur->field.first);
  eraseList(iter.cur);

  return make_iterator(next);
}

SORTED_MAP_TEMPLATE
typename map_list<Key, T, U>::iterator map_list<Key, T, U>::erase(
    const_iterator iter) noexcept {
  list_item *next = iter.cur->next;

  eraseMap(iter.cur->field.first);
  eraseList(iter.cur);

  return make_iterator(next);
}

SORTED_MAP_TEMPLATE
typename map_list<Key, T, U>::value_type &
map_list<Key, T, U>::front() noexcept {
  if (LIKELY(map.size() > 0)) {
    return listHead->next->field;
  }

  return listTail->field;
}

SORTED_MAP_TEMPLATE
const typename map_list<Key, T, U>::value_type &map_list<Key, T, U>::front()
    const noexcept {
  if (LIKELY(map.size() > 0)) {
    return listHead->next->field;
  }

  return listTail->field;
}

SORTED_MAP_TEMPLATE
typename map_list<Key, T, U>::value_type &map_list<Key, T, U>::back() noexcept {
  if (LIKELY(map.size() > 0)) {
    return listTail->prev->field;
  }

  return listTail->field;
}

SORTED_MAP_TEMPLATE
const typename map_list<Key, T, U>::value_type &map_list<Key, T, U>::back()
    const noexcept {
  if (LIKELY(map.size() > 0)) {
    return listTail->prev->field;
  }

  return listTail->field;
}

SORTED_MAP_TEMPLATE
void map_list<Key, T, U>::clear() noexcept {
  // Free all objects
  for (auto &iter : map) {
    delete iter.second;
  }

  // Clear structures
  map.clear();

  listHead->next = listTail;
  listTail->prev = listHead;
}

SORTED_MAP_TEMPLATE
typename map_list<Key, T, U>::iterator map_list<Key, T, U>::begin() noexcept {
  return make_iterator(listHead->next);
}

SORTED_MAP_TEMPLATE
typename map_list<Key, T, U>::iterator map_list<Key, T, U>::end() noexcept {
  return make_iterator(listTail);
}

SORTED_MAP_TEMPLATE
typename map_list<Key, T, U>::const_iterator map_list<Key, T, U>::begin()
    const noexcept {
  return make_iterator(listHead->next);
}

SORTED_MAP_TEMPLATE
typename map_list<Key, T, U>::const_iterator map_list<Key, T, U>::end()
    const noexcept {
  return make_iterator(listTail);
}

SORTED_MAP_TEMPLATE
map_map<Key, T, U>::map_map(Compare c) : map_list<Key, T>(), func(c) {}

SORTED_MAP_TEMPLATE
map_map<Key, T, U>::map_map(map_map &&rhs) noexcept {
  *this = std::move(rhs);
}

SORTED_MAP_TEMPLATE
map_map<Key, T, U>::~map_map() {}

SORTED_MAP_TEMPLATE
map_map<Key, T, U> &map_map<Key, T, U>::operator=(map_map &&rhs) {
  if (this != &rhs) {
    this->map = std::move(rhs.map);
    this->listHead = std::exchange(rhs.listHead, nullptr);
    this->listTail = std::exchange(rhs.listTail, nullptr);
    this->func = std::move(rhs.func);
  }

  return *this;
}

SORTED_MAP_TEMPLATE
std::pair<typename map_map<Key, T, U>::iterator, bool>
map_map<Key, T, U>::insert(const key_type &key,
                           const mapped_type &value) noexcept {
  if (LIKELY(this->map.count(key) == 0)) {
    // Find where to insert
    list_item *prev = this->listHead;

    while (prev != this->listTail) {
      // prev->next->value is nullptr when prev->next is &listTail
      if (prev->next == this->listTail ||
          func(value, prev->next->field.second)) {
        break;
      }

      prev = prev->next;
    }

    // Insert item to list
    auto entry = this->insertList(prev, std::make_pair(key, value));

    // Insert item to map
    this->insertMap(key, entry);

    return std::make_pair(this->make_iterator(entry), true);
  }

  return std::make_pair(this->end(), false);
}

}  // namespace SimpleSSD

#endif
