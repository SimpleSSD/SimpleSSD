// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "ftl/allocator/victim_selection.hh"

#include "ftl/allocator/abstract_allocator.hh"

namespace SimpleSSD::FTL::BlockAllocator {

AbstractVictimSelection::AbstractVictimSelection(AbstractAllocator *p)
    : pAllocator(p) {}

AbstractAllocator::~AbstractAllocator() {}

}  // namespace SimpleSSD::FTL::BlockAllocator
