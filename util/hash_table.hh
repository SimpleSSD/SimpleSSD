/*
 * Copyright (C) 2017 CAMELab
 *
 * This file is part of SimpleSSD.
 *
 * SimpleSSD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SimpleSSD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SimpleSSD.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __UTIL_HASH_TABLE__
#define __UTIL_HASH_TABLE__

#include <functional>

#include "util/list.hh"

namespace SimpleSSD {

template <typename KeyType, typename ValueType>
class HashTable {
 private:
  struct Item {
    KeyType key;
    ValueType value;
  };

  List<Item> *hashTable;
  std::function<uint64_t(KeyType &, bool)> hashFunction;
  std::function<bool(KeyType &, KeyType &)> compareFunction;  // True if a >= b

 public:
  HashTable(std::function<uint64_t(KeyType, bool)> &hf,
            std::function<bool(KeyType &, KeyType &)> &cf)
      : hashFunction(hf), compareFunction(cf) {
    uint64_t max = hashFunction(NULL, true);

    hashTable = new List<Item>[max]();
  }

  ~HashTable() { delete hashTable; }

  void set(KeyType &key, ValueType &val) {
    uint64_t bucket = hashFunction(key, false);
    auto iter = hashTable[bucket].begin();
    Item item;

    for (; iter != nullptr; iter++) {
      if (compareFunction(iter->val().key, key)) {
        if (iter->val().key == key) {
          iter->val().value = val;
        }
        else {
          item.key = key;
          item.value = val;

          hashTable[bucket].insert(iter, item);
        }

        break;
      }
    }
  }

  bool remove(KeyType &key) {
    uint64_t bucket = hashFunction(key, false);
    auto iter = hashTable[bucket].begin();

    for (; iter != nullptr; iter++) {
      if (iter->val().key == key) {
        hashTable[bucket].erase(iter);

        break;
      }
    }
  }

  ValueType &get(KeyType &key) {
    uint64_t bucket = hashFunction(key, false);
    auto iter = hashTable[bucket].begin();

    for (; iter != nullptr; iter++) {
      if (iter->val().key == key) {
        return iter->val().value;
      }
    }
  }
};

}  // namespace SimpleSSD

#endif
