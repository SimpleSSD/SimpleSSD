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

#include "util/disk.hh"

#include <cstring>
#include <filesystem>

#define SECTOR_SIZE 512

namespace SimpleSSD {

Disk::Disk(ObjectData &o) : Object(o), diskSize(0) {}

Disk::~Disk() {
  close();
}

uint64_t Disk::open(std::string path, uint64_t desiredSize) noexcept {
  filename = path;

  // Validate size
  std::filesystem::path fspath(path);

  if (std::filesystem::exists(path)) {
    if (std::filesystem::is_regular_file(path)) {
      diskSize = std::filesystem::file_size(path);
    }
    else {
      panic("Provided file is not a regular file.");
    }
  }
  else {
    // Create zero-sized file
    disk.open(filename, std::ios::out | std::ios::binary);
    disk.close();

    try {
      std::filesystem::resize_file(path, desiredSize);
    }
    catch (std::exception &) {
      panic("Failed to create and resize disk image file.");
    }
  }

  // Open file
  disk.open(filename, std::ios::in | std::ios::out | std::ios::binary);

  if (!disk.is_open()) {
    // panic("failed to open file");
  }

  return diskSize;
}

void Disk::close() noexcept {
  if (disk.is_open()) {
    disk.close();
  }
}

uint32_t Disk::read(uint64_t offset, uint32_t size, uint8_t *buffer) noexcept {
  uint16_t ret = 0;

  if (disk.is_open()) {
    if (offset + size > diskSize) {
      if (offset >= diskSize) {
        size = 0;
      }
      else {
        size = diskSize - offset;
      }
    }

    if (size > 0) {
      disk.seekg(offset, std::ios::beg);

      if (!disk.good()) {
        // panic("nvme_disk: Fail to seek to %" PRIu64 "\n", slba);
      }

      disk.read((char *)buffer, size);
    }

    // DPRINTF(NVMeDisk, "DISK    | READ  | BYTE %016" PRIX64 " + %X\n",
    //         slba, nlblk * sectorSize);

    ret = size;
  }

  return ret;
}

uint32_t Disk::write(uint64_t offset, uint32_t size, uint8_t *buffer) noexcept {
  uint16_t ret = 0;

  if (disk.is_open()) {
    disk.seekp(offset, std::ios::beg);

    if (!disk.good()) {
      // panic("nvme_disk: Fail to seek to %" PRIu64 "\n", slba);
    }

    if (offset + size > diskSize) {
      if (offset >= diskSize) {
        size = 0;
      }
      else {
        size = diskSize - offset;
      }
    }

    if (size > 0) {
      disk.write((char *)buffer, size);
    }

    // DPRINTF(NVMeDisk, "DISK    | WRITE | BYTE %016" PRIX64 " + %X\n", slba,
    //         offset);

    ret = size;
  }

  return ret;
}

uint32_t Disk::erase(uint64_t, uint32_t size) noexcept {
  return size;
}

void Disk::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Disk::getStatValues(std::vector<double> &) noexcept {}

void Disk::resetStatValues() noexcept {}

void Disk::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, diskSize);

  uint64_t size = filename.size();
  BACKUP_SCALAR(out, size);
  BACKUP_BLOB(out, filename.c_str(), size);
}

void Disk::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, diskSize);

  uint64_t size;
  RESTORE_SCALAR(in, size);

  filename.resize(size);

  RESTORE_BLOB(in, filename.c_str(), size);

  // Okay, try to open.
  open(filename, diskSize);
}

CoWDisk::CoWDisk(ObjectData &o) : Disk(o) {}

CoWDisk::~CoWDisk() {
  close();
}

void CoWDisk::close() noexcept {
  table.clear();

  Disk::close();
}

uint32_t CoWDisk::read(uint64_t offset, uint32_t size,
                       uint8_t *buffer) noexcept {
  uint16_t read = 0;

  panic_if(size % SECTOR_SIZE, "Read to disk is not aligned.");

  for (uint64_t i = 0; i < size / SECTOR_SIZE; i++) {
    auto lba = offset / SECTOR_SIZE + i;
    auto block = table.find(lba);

    if (block != table.end()) {
      memcpy(buffer + i * SECTOR_SIZE, block->second.data(), SECTOR_SIZE);
      read++;
    }
    else {
      read +=
          Disk::read(lba * SECTOR_SIZE, SECTOR_SIZE, buffer + i * SECTOR_SIZE) /
          SECTOR_SIZE;
    }
  }

  return read * SECTOR_SIZE;
}

uint32_t CoWDisk::write(uint64_t offset, uint32_t size,
                        uint8_t *buffer) noexcept {
  uint16_t write = 0;

  panic_if(size % SECTOR_SIZE, "Write to disk is not aligned.");

  for (uint64_t i = 0; i < size / SECTOR_SIZE; i++) {
    auto lba = offset / SECTOR_SIZE + i;
    auto block = table.find(lba);

    if (block != table.end()) {
      memcpy(block->second.data(), buffer + i * SECTOR_SIZE, SECTOR_SIZE);
    }
    else {
      std::vector<uint8_t> data;

      data.resize(SECTOR_SIZE);
      memcpy(data.data(), buffer + i * SECTOR_SIZE, SECTOR_SIZE);

      table.emplace(std::make_pair(lba, data));
    }

    write++;
  }

  return write * SECTOR_SIZE;
}

void CoWDisk::createCheckpoint(std::ostream &out) const noexcept {
  Disk::createCheckpoint(out);

  uint64_t size = table.size();
  BACKUP_SCALAR(out, size);

  for (auto &iter : table) {
    BACKUP_SCALAR(out, iter.first);

    size = iter.second.size();
    BACKUP_SCALAR(out, size);
    BACKUP_BLOB(out, iter.second.data(), size);
  }
}

void CoWDisk::restoreCheckpoint(std::istream &in) noexcept {
  uint64_t lba;

  Disk::restoreCheckpoint(in);

  uint64_t size;
  RESTORE_SCALAR(in, size);

  table.reserve(size);

  for (uint64_t i = 0; i < size; i++) {
    RESTORE_SCALAR(in, lba);

    auto ret = table.emplace(lba, std::vector<uint8_t>());

    panic_if(!ret.second, "Failed to restore disk contents");

    RESTORE_SCALAR(in, lba);
    ret.first->second.resize(lba);
    RESTORE_BLOB(in, ret.first->second.data(), lba);
  }
}

MemDisk::MemDisk(ObjectData &o) : CoWDisk(o) {}

MemDisk::~MemDisk() {
  close();
}

uint64_t MemDisk::open(std::string, uint64_t size) noexcept {
  diskSize = size;

  return size;
}

void MemDisk::close() noexcept {
  table.clear();
}

uint32_t MemDisk::read(uint64_t offset, uint32_t size,
                       uint8_t *buffer) noexcept {
  uint16_t read = 0;

  panic_if(size % SECTOR_SIZE, "Read to disk is not aligned.");

  for (uint64_t i = 0; i < size / SECTOR_SIZE; i++) {
    auto lba = offset / SECTOR_SIZE + i;
    auto block = table.find(lba);

    if (block != table.end()) {
      memcpy(buffer + i * SECTOR_SIZE, block->second.data(), SECTOR_SIZE);
    }
    else {
      memset(buffer + i * SECTOR_SIZE, 0, SECTOR_SIZE);
    }

    read++;
  }

  return read * SECTOR_SIZE;
}

uint32_t MemDisk::write(uint64_t offset, uint32_t size,
                        uint8_t *buffer) noexcept {
  uint16_t write = 0;

  panic_if(size % SECTOR_SIZE, "Write to disk is not aligned.");

  for (uint64_t i = 0; i < size / SECTOR_SIZE; i++) {
    auto lba = offset / SECTOR_SIZE + i;
    auto block = table.find(lba);

    if (block != table.end()) {
      memcpy(block->second.data(), buffer + i * SECTOR_SIZE, SECTOR_SIZE);
    }
    else {
      std::vector<uint8_t> data;

      data.resize(SECTOR_SIZE);
      memcpy(data.data(), buffer + i * SECTOR_SIZE, SECTOR_SIZE);

      table.emplace(std::make_pair(lba, data));
    }

    write++;
  }

  return write * SECTOR_SIZE;
}

uint32_t MemDisk::erase(uint64_t offset, uint32_t size) noexcept {
  uint16_t erase = 0;

  panic_if(size % SECTOR_SIZE, "Erase to disk is not aligned.");

  for (uint64_t i = 0; i < size / SECTOR_SIZE; i++) {
    auto lba = offset / SECTOR_SIZE + i;
    auto block = table.find(lba);

    if (block != table.end()) {
      table.erase(block);
    }

    erase++;
  }

  return erase * SECTOR_SIZE;
}

}  // namespace SimpleSSD
