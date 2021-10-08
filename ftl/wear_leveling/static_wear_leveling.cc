// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/wear_leveling/static_wear_leveling.hh"

#include "ftl/allocator/abstract_allocator.hh"

namespace SimpleSSD::FTL::WearLeveling {

StaticWearLeveling::StaticWearLeveling(ObjectData &o, FTLObjectData &fo,
                                       FIL::FIL *fil)
    : AbstractWearLeveling(o, fo, fil), beginAt(0) {
  resetStatValues();
}

StaticWearLeveling::~StaticWearLeveling() {}

void StaticWearLeveling::blockEraseCallback(uint64_t now, const PSBN &erased) {
  // TODO
}

void StaticWearLeveling::getStatList(std::vector<Stat> &list,
                                     std::string prefix) noexcept {
  list.emplace_back(prefix + "wear_leveling.block", "Total reclaimed blocks");
  list.emplace_back(prefix + "wear_leveling.copy", "Total valid page copy");
}

void StaticWearLeveling::getStatValues(std::vector<double> &values) noexcept {
  values.push_back((double)stat.erasedBlocks);
  values.push_back((double)stat.copiedPages);
}

void StaticWearLeveling::resetStatValues() noexcept {
  stat.copiedPages = 0;
  stat.erasedBlocks = 0;
}

void StaticWearLeveling::createCheckpoint(std::ostream &out) const noexcept {
  AbstractWearLeveling::createCheckpoint(out);

  BACKUP_SCALAR(out, beginAt);
  BACKUP_SCALAR(out, stat);
}

void StaticWearLeveling::restoreCheckpoint(std::istream &in) noexcept {
  AbstractWearLeveling::restoreCheckpoint(in);

  RESTORE_SCALAR(in, beginAt);
  RESTORE_SCALAR(in, stat);
}

}  // namespace SimpleSSD::FTL::WearLeveling
