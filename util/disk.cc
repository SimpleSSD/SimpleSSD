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

namespace SimpleSSD {

Disk::Disk(ObjectData &o) : Object(o), diskSize(0), sectorSize(0) {}

Disk::~Disk() {
  close();
}

uint64_t Disk::open(std::string path, uint64_t desiredSize,
                    uint32_t lbaSize) noexcept {
  filename = path;
  sectorSize = lbaSize;

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
    catch (std::exception &e) {
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

uint16_t Disk::read(uint64_t slba, uint16_t nlblk, uint8_t *buffer) noexcept {
  uint16_t ret = 0;

  if (disk.is_open()) {
    uint64_t avail;

    slba *= sectorSize;
    avail = nlblk * sectorSize;

    if (slba + avail > diskSize) {
      if (slba >= diskSize) {
        avail = 0;
      }
      else {
        avail = diskSize - slba;
      }
    }

    if (avail > 0) {
      disk.seekg(slba, std::ios::beg);
      if (!disk.good()) {
        // panic("nvme_disk: Fail to seek to %" PRIu64 "\n", slba);
      }

      disk.read((char *)buffer, avail);
    }

    memset(buffer + avail, 0, nlblk * sectorSize - avail);

    // DPRINTF(NVMeDisk, "DISK    | READ  | BYTE %016" PRIX64 " + %X\n",
    //         slba, nlblk * sectorSize);

    ret = nlblk;
  }

  return ret;
}

uint16_t Disk::write(uint64_t slba, uint16_t nlblk, uint8_t *buffer) noexcept {
  uint16_t ret = 0;

  if (disk.is_open()) {
    slba *= sectorSize;

    disk.seekp(slba, std::ios::beg);
    if (!disk.good()) {
      // panic("nvme_disk: Fail to seek to %" PRIu64 "\n", slba);
    }

    uint64_t offset = disk.tellp();
    disk.write((char *)buffer, sectorSize * nlblk);
    offset = (uint64_t)disk.tellp() - offset;

    // DPRINTF(NVMeDisk, "DISK    | WRITE | BYTE %016" PRIX64 " + %X\n", slba,
    //         offset);

    ret = offset / sectorSize;
  }

  return ret;
}

uint16_t Disk::erase(uint64_t, uint16_t nlblk) noexcept {
  return nlblk;
}

void Disk::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Disk::getStatValues(std::vector<double> &) noexcept {}

void Disk::resetStatValues() noexcept {}

void Disk::createCheckpoint(std::ostream &out) noexcept {
  BACKUP_SCALAR(out, diskSize);
  BACKUP_SCALAR(out, sectorSize);

  uint64_t size = filename.size();
  BACKUP_SCALAR(out, size);
  BACKUP_BLOB(out, filename.c_str(), size);
}

void Disk::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, diskSize);
  RESTORE_SCALAR(in, sectorSize);

  uint64_t size;
  RESTORE_SCALAR(in, size);

  filename.resize(size);

  RESTORE_BLOB(in, filename.c_str(), size);

  // Okay, try to open.
  open(filename, diskSize, sectorSize);
}

CoWDisk::CoWDisk(ObjectData &o) : Disk(o) {}

CoWDisk::~CoWDisk() {
  close();
}

void CoWDisk::close() noexcept {
  table.clear();

  Disk::close();
}

uint16_t CoWDisk::read(uint64_t slba, uint16_t nlblk,
                       uint8_t *buffer) noexcept {
  uint16_t read = 0;

  for (uint64_t i = 0; i < nlblk; i++) {
    auto block = table.find(slba + i);

    if (block != table.end()) {
      memcpy(buffer + i * sectorSize, block->second.data(), sectorSize);
      read++;
    }
    else {
      read += Disk::read(slba + i, 1, buffer + i * sectorSize);
    }
  }

  return read;
}

uint16_t CoWDisk::write(uint64_t slba, uint16_t nlblk,
                        uint8_t *buffer) noexcept {
  uint16_t write = 0;

  for (uint64_t i = 0; i < nlblk; i++) {
    auto block = table.find(slba + i);

    if (block != table.end()) {
      memcpy(block->second.data(), buffer + i * sectorSize, sectorSize);
    }
    else {
      std::vector<uint8_t> data;

      data.resize(sectorSize);
      memcpy(data.data(), buffer + i * sectorSize, sectorSize);

      table.insert({slba + i, data});
    }

    write++;
  }

  return write;
}

void CoWDisk::createCheckpoint(std::ostream &out) noexcept {
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

uint64_t MemDisk::open(std::string, uint64_t size, uint32_t lbaSize) noexcept {
  diskSize = size;
  sectorSize = lbaSize;

  return size;
}

void MemDisk::close() {
  table.clear();
}

uint16_t MemDisk::read(uint64_t slba, uint16_t nlblk,
                       uint8_t *buffer) noexcept {
  uint16_t read = 0;

  for (uint64_t i = 0; i < nlblk; i++) {
    auto block = table.find(slba + i);

    if (block != table.end()) {
      memcpy(buffer + i * sectorSize, block->second.data(), sectorSize);
    }
    else {
      memset(buffer + i * sectorSize, 0, sectorSize);
    }

    read++;
  }

  return read;
}

uint16_t MemDisk::write(uint64_t slba, uint16_t nlblk,
                        uint8_t *buffer) noexcept {
  uint16_t write = 0;

  for (uint64_t i = 0; i < nlblk; i++) {
    auto block = table.find(slba + i);

    if (block != table.end()) {
      memcpy(block->second.data(), buffer + i * sectorSize, sectorSize);
    }
    else {
      std::vector<uint8_t> data;

      data.resize(sectorSize);
      memcpy(data.data(), buffer + i * sectorSize, sectorSize);

      table.insert({slba + i, data});
    }

    write++;
  }

  return write;
}

uint16_t MemDisk::erase(uint64_t slba, uint16_t nlblk) noexcept {
  uint16_t erase = 0;

  for (uint64_t i = 0; i < nlblk; i++) {
    auto block = table.find(slba + i);

    if (block != table.end()) {
      table.erase(block);
    }

    erase++;
  }

  return erase;
}

}  // namespace SimpleSSD
