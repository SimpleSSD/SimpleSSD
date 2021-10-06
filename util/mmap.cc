// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "util/mmap.hh"

#ifdef _MSC_VER
#else

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#endif

namespace SimpleSSD {

#ifdef _MSC_VER
#else

void *openFileMapping(const char *path, uint64_t *psize, bool create,
                      bool cow) {
  int fd;
  struct stat64 st;
  uint64_t size = 0;

  // Requires size parameter
  if (psize == nullptr) {
    return nullptr;
  }

  if (stat64(path, &st) == 0) {
    // File exists
    if (S_ISREG(st.st_mode)) {
      size = st.st_size;
    }
    else {
      return nullptr;
    }

    *psize = size;

    fd = open(path, O_RDWR);
  }
  else if (create) {
    // File not exists
    fd = open(path, O_RDWR | O_CREAT, 0644);

    // Resize file
    if (fd != -1 && ftruncate64(fd, *psize) != 0) {
      close(fd);

      return nullptr;
    }
  }
  else {
    return nullptr;
  }

  // Check file
  if (fd == -1) {
    return nullptr;
  }

  // Try mapping
  int flags = 0;

  if (cow) {
    flags = MAP_PRIVATE;
  }
  else {
    flags = MAP_SHARED;
  }

  void *ptr = mmap64(nullptr, *psize, PROT_READ | PROT_WRITE, flags, fd, 0);

  if (ptr == MAP_FAILED) {
    ptr = nullptr;
  }

  // Close file
  close(fd);

  return ptr;
}

void closeFileMapping(void *ptr, uint64_t size) {
  munmap(ptr, size);
}

#endif

}  // namespace SimpleSSD
