// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "fil/pal/convert.hh"

namespace SimpleSSD::FIL {

Convert::Convert(ObjectData &o) : Object(o), isPowerOfTwo(false) {
  auto nand = object.config->getNANDStructure();
  channel =
      (uint32_t)readConfigUint(Section::FlashInterface, Config::Key::Channel);
  way = (uint32_t)readConfigUint(Section::FlashInterface, Config::Key::Way);
  die = nand->die;
  plane = nand->plane;
  block = nand->block;
  page = nand->page;

  if (popcount32(channel) == 1 && popcount32(way) == 1 &&
      popcount32(die) == 1 && popcount32(plane) == 1 &&
      popcount32(block) == 1 && popcount32(page) == 1) {
    isPowerOfTwo = true;

    maskChannel = channel - 1;
    maskWay = way - 1;
    maskDie = die - 1;
    maskPlane = plane - 1;
    maskBlock = block - 1;
    maskPage = page - 1;
  }
}

ConvertFunction Convert::getConvertion() {
  auto nand = object.config->getNANDStructure();

  if (isPowerOfTwo) {
    uint8_t sum = 0;

    for (uint8_t i = 0; i < 4; i++) {
      switch (nand->pageAllocation[i]) {
        case PageAllocation::Channel:
          shiftChannel = sum;
          sum += popcount32(maskChannel);

          break;
        case PageAllocation::Way:
          shiftWay = sum;
          sum += popcount32(maskWay);

          break;
        case PageAllocation::Die:
          shiftDie = sum;
          sum += popcount32(maskDie);

          break;
        case PageAllocation::Plane:
          shiftPlane = sum;
          sum += popcount32(maskPlane);

          break;
      }
    }

    shiftBlock = sum;
    sum += popcount32(shiftBlock);
    shiftPage = sum;

    return [this](Request &req, ::CPDPBP &addr) {
      addr.Channel = (req.address >> shiftChannel) & maskChannel;
      addr.Package = (req.address >> shiftWay) & maskWay;
      addr.Die = (req.address >> shiftDie) & maskDie;
      addr.Plane = (req.address >> shiftPlane) & maskPlane;
      addr.Block = (req.address >> shiftBlock) & maskBlock;
      addr.Page = (req.address >> shiftPage) & maskPage;
    };
  }
  else {
    uint64_t level[4] = {0, 0, 0, 0};
    uint8_t offset[4] = {0, 0, 0, 0};

    for (uint8_t i = 0; i < 4; i++) {
      switch (nand->pageAllocation[i]) {
        case PageAllocation::Channel:
          level[i] = channel;
          offset[i] = 0;

          break;
        case PageAllocation::Way:
          level[i] = way;
          offset[i] = 1;

          break;
        case PageAllocation::Die:
          level[i] = die;
          offset[i] = 2;

          break;
        case PageAllocation::Plane:
          level[i] = plane;
          offset[i] = 3;

          break;
      }
    }

    return [level, offset, block = this->block, page = this->page](
               Request &req, ::CPDPBP &addr) {
      uint32_t *values = (uint32_t *)&addr;

      values[offset[0]] = req.address % level[0];
      req.address /= level[0];
      values[offset[1]] = req.address % level[1];
      req.address /= level[1];
      values[offset[2]] = req.address % level[2];
      req.address /= level[2];
      values[offset[3]] = req.address % level[3];
      req.address /= level[3];
      values[4] = req.address % block;
      req.address /= block;
      values[5] = req.address % page;
    };
  }
}

void Convert::getStatList(std::vector<Stat> &, std::string) noexcept {}

void Convert::getStatValues(std::vector<double> &) noexcept {}

void Convert::resetStatValues() noexcept {}

void Convert::createCheckpoint(std::ostream &out) const noexcept {
  BACKUP_SCALAR(out, isPowerOfTwo);
  BACKUP_SCALAR(out, maskChannel);
  BACKUP_SCALAR(out, maskWay);
  BACKUP_SCALAR(out, maskDie);
  BACKUP_SCALAR(out, maskPlane);
  BACKUP_SCALAR(out, maskBlock);
  BACKUP_SCALAR(out, maskPage);
  BACKUP_SCALAR(out, shiftChannel);
  BACKUP_SCALAR(out, shiftWay);
  BACKUP_SCALAR(out, shiftDie);
  BACKUP_SCALAR(out, shiftPlane);
  BACKUP_SCALAR(out, shiftBlock);
  BACKUP_SCALAR(out, shiftPage);
  BACKUP_SCALAR(out, channel);
  BACKUP_SCALAR(out, way);
  BACKUP_SCALAR(out, die);
  BACKUP_SCALAR(out, plane);
  BACKUP_SCALAR(out, block);
  BACKUP_SCALAR(out, page);
}

void Convert::restoreCheckpoint(std::istream &in) noexcept {
  RESTORE_SCALAR(in, isPowerOfTwo);
  RESTORE_SCALAR(in, maskChannel);
  RESTORE_SCALAR(in, maskWay);
  RESTORE_SCALAR(in, maskDie);
  RESTORE_SCALAR(in, maskPlane);
  RESTORE_SCALAR(in, maskBlock);
  RESTORE_SCALAR(in, maskPage);
  RESTORE_SCALAR(in, shiftChannel);
  RESTORE_SCALAR(in, shiftWay);
  RESTORE_SCALAR(in, shiftDie);
  RESTORE_SCALAR(in, shiftPlane);
  RESTORE_SCALAR(in, shiftBlock);
  RESTORE_SCALAR(in, shiftPage);
  RESTORE_SCALAR(in, channel);
  RESTORE_SCALAR(in, way);
  RESTORE_SCALAR(in, die);
  RESTORE_SCALAR(in, plane);
  RESTORE_SCALAR(in, block);
  RESTORE_SCALAR(in, page);
}

}  // namespace SimpleSSD::FIL
