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

#ifndef __UTIL_LIST__
#define __UTIL_LIST__

#include <cerrno>
#include <cinttypes>
#include <cstdlib>
#include <cstring>

namespace SimpleSSD {

/**
 * This list uses calloc/free to allocate memory
 * So if you pass object (not pointer), constructor will not called
 */
template <typename T>
class List {
 public:
  class Iterator {
   private:
    Iterator *before;
    Iterator *next;
    T value;

   public:
    Iterator() : before(nullptr), next(nullptr), value(NULL) {}

    Iterator &operator++() {
      before = next->_before;
      value = next->_value;
      next = next->_next;

      return *this;
    }

    Iterator operator++(int) {
      List<T>::Iterator tmp(*this);
      operator++();

      return tmp;
    }

    T &val() { return value; }
  };

  friend class Iterator;

 private:
  Iterator *head;
  Iterator *tail;
  uint64_t length;

 public:
  List() : head(nullptr), tail(nullptr), length(0) {}

  uint64_t size() { return length; }

  void push_back(T val) {
    insert(nullptr, val);
  }

  T pop_back() {
    if (length > 0) {
      Iterator *iter = tail;
      T ret = iter->value;

      erase(iter);

      return ret;
    }
    else {
      errno = -ERANGE;

      return T();
    }
  }

  void push_front(T val) {
    insert(head, val);
  }

  T pop_front() {
    if (length > 0) {
      Iterator *iter = head;
      T ret = iter->value;

      erase(iter);

      return ret;
    }
    else {
      errno = -ERANGE;
      return T();
    }
  }

  Iterator *begin() { return head; }

  Iterator *end() { return nullptr; }

  Iterator *insert(Iterator *next, T val) {
    Iterator *iter = new Iterator();

    if (next != nullptr) {
      Iterator *before = next->before;

      iter->before = before;
      iter->next = next;
      next->before = iter;

      if (before != nullptr) {
        before->next = iter;
      }
      else {
        head = iter;
      }
    }
    else {
      head = iter;
      tail = iter;
    }

    memcpy(&iter->value, &val, sizeof(T));
    length++;

    return iter;
  }

  Iterator *erase(Iterator *iter) {
    Iterator *ret = iter->next;

    if (iter->next) {
      iter->next->before = iter->before;
    }
    else {
      tail = iter->before;
    }

    if (iter->before) {
      iter->before->next = iter->next;
    }
    else {
      head = iter->next;
    }

    length--;
    delete iter;

    return ret;
  }
};

}  // namespace SimpleSSD

#endif
