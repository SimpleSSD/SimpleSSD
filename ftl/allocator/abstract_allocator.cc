// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/allocator/abstract_allocator.hh"

#include "ftl/mapping/abstract_mapping.hh"

namespace SimpleSSD::FTL::BlockAllocator {

AbstractAllocator::AbstractAllocator(ObjectData &o, FTLObjectData &fo)
    : Object(o), ftlobject(fo), param(nullptr) {}

AbstractAllocator::~AbstractAllocator() {}

void AbstractAllocator::initialize() {
  param = ftlobject.pMapping->getInfo();
}

}  // namespace SimpleSSD::FTL::BlockAllocator
