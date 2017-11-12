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

#ifndef __UTIL_VECTOR__
#define __UTIL_VECTOR__

#include <cerrno>
#include <cinttypes>
#include <cstdlib>
#include <cstring>

namespace SimpleSSD {

#define ALLOC_UNIT 64

/**
 * This vector uses calloc/free to allocate memory
 * So if you pass object (not pointer), constructor will not called
 */
template <typename T>
class Vector {
 private:
  T *data;
  uint64_t length;
  uint64_t capacity;

 public:
  Vector() {
    data = (T *)calloc(ALLOC_UNIT, sizeof(T));

    if (data == NULL) {
      errno = -ENOMEM;
    }
    else {
      errno = 0;
      length = 0;
      capacity = ALLOC_UNIT;
    }
  }

  Vector(uint64_t count) {
    capacity = (count / ALLOC_UNIT + 1) * ALLOC_UNIT;
    length = count;

    data = (T *)calloc(capacity, sizeof(T));

    if (data == NULL) {
      errno = -ENOMEM;
    }
    else {
      errno = 0;
    }
  }

  ~Vector() {
    errno = 0;
    free(data);
  }

  T &at(uint64_t idx) {
    if (idx < length) {
      errno = 0;
    }
    else {
      errno = -ERANGE;
    }

    return data[idx];
  }

  T &operator[](uint64_t idx) { return this->at(idx); }

  uint64_t size() { return length; }

  void resize(uint64_t count) {
    if (count >= capacity) {
      capacity = (count / ALLOC_UNIT + 1) * ALLOC_UNIT;

      T *ptr = (T *)realloc(data, capacity * sizeof(T));

      if (ptr == NULL) {
        errno = -ENOMEM;
        capacity = (length / ALLOC_UNIT + 1) * ALLOC_UNIT;
      }
      else {
        errno = 0;
        data = ptr;
        memset(data + length, 0, capacity - length);
      }
    }

    length = count;
  }

  void push_back(T val) { insert(length, val); }

  T pop_back() {
    if (length > 0) {
      T val;

      memcpy(&val, data + length - 1, sizeof(T));
      erase(length - 1);

      return val;
    }
    else {
      errno = -ERANGE;

      return T();
    }
  }

  void insert(uint64_t idx, T val) {
    resize(length + 1);

    if (errno == 0) {
      memmove(data + idx, data + idx + 1, (length - idx - 1) * sizeof(T));
      memcpy(data + idx, &val, sizeof(T));
    }
  }

  void erase(uint64_t idx) {
    if (idx < length) {
      memmove(data + idx + 1, data + idx, (length-- - idx - 1) * sizeof(T));
      memset(data + length, 0, sizeof(T));
    }
    else {
      errno = -ERANGE;
    }
  }
};

}  // namespace SimpleSSD

#endif
