// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "fil/nvm/pal/convert.hh"

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
        default:
          break;
      }
    }

    shiftBlock = sum;
    sum += popcount32(maskBlock);
    shiftPage = sum;

    return [this](PPN ppn, ::CPDPBP &addr) {
      addr.Channel = (uint32_t)((ppn >> shiftChannel) & maskChannel);
      addr.Package = (uint32_t)((ppn >> shiftWay) & maskWay);
      addr.Die = (uint32_t)((ppn >> shiftDie) & maskDie);
      addr.Plane = (uint32_t)((ppn >> shiftPlane) & maskPlane);
      addr.Block = (uint32_t)((ppn >> shiftBlock) & maskBlock);
      addr.Page = (uint32_t)((ppn >> shiftPage) & maskPage);
    };
  }
  else {
    uint64_t level[4] = {0, 0, 0, 0};
    uint8_t ppnIndex[4] = {0, 0, 0, 0};

    for (uint8_t i = 0; i < 4; i++) {
      switch (nand->pageAllocation[i]) {
        case PageAllocation::Channel:
          level[i] = channel;
          ppnIndex[i] = 0;

          break;
        case PageAllocation::Way:
          level[i] = way;
          ppnIndex[i] = 1;

          break;
        case PageAllocation::Die:
          level[i] = die;
          ppnIndex[i] = 2;

          break;
        case PageAllocation::Plane:
          level[i] = plane;
          ppnIndex[i] = 3;

          break;
        default:
          break;
      }
    }

    return [level, ppnIndex, block = this->block, page = this->page](
               PPN ppn, ::CPDPBP &addr) {
      uint32_t *values = (uint32_t *)&addr;

      values[ppnIndex[0]] = (uint32_t)(ppn % level[0]);
      ppn /= level[0];
      values[ppnIndex[1]] = (uint32_t)(ppn % level[1]);
      ppn /= level[1];
      values[ppnIndex[2]] = (uint32_t)(ppn % level[2]);
      ppn /= level[2];
      values[ppnIndex[3]] = (uint32_t)(ppn % level[3]);
      ppn /= level[3];
      values[4] = (uint32_t)(ppn % block);
      ppn /= block;
      values[5] = (uint32_t)(ppn % page);
    };
  }
}

void Convert::getBlockAlignedPPN(PPN &ppn) {
  if (isPowerOfTwo) {
    uint64_t mask = std::numeric_limits<uint64_t>::max() << shiftPage;

    ppn &= ~mask;
  }
  else {
    uint64_t div = channel * way * die * plane * block;

    ppn = static_cast<PPN>(ppn % div);
  }
}

void Convert::increasePage(PPN &ppn) {
  if (isPowerOfTwo) {
    uint64_t add = (uint64_t)1ull << shiftPage;

    ppn += add;
  }
  else {
    uint64_t add = channel * way * die * plane * block;

    ppn += add;
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
