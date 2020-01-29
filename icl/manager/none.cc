// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "icl/manager/none.hh"

namespace SimpleSSD::ICL {

NoCache::NoCache(ObjectData &o, FTL::FTL *f) : AbstractManager(o, f) {}

NoCache::~NoCache() {}

void NoCache::read(SubRequest *) {
  // TODO: FTL
}

void NoCache::write(SubRequest *) {
  // TODO: FTL
}

void NoCache::flush(SubRequest *) {}

void NoCache::erase(SubRequest *) {
  // TODO: FTL
}

void NoCache::dmaDone(SubRequest *) {}

void NoCache::drain(std::vector<FlushContext> &) {}

void NoCache::getStatList(std::vector<Stat> &, std::string) noexcept {}

void NoCache::getStatValues(std::vector<double> &) noexcept {}

void NoCache::resetStatValues() noexcept {}

void NoCache::createCheckpoint(std::ostream &) const noexcept {}

void NoCache::restoreCheckpoint(std::istream &) noexcept {}

}  // namespace SimpleSSD::ICL
