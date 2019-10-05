// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/hil.hh"

#ifndef HIL_TEMPLATE
#define HIL_TEMPLATE                                                           \
  template <class LPN, std::enable_if_t<std::is_unsigned_v<LPN>> = 0>

namespace SimpleSSD::HIL {

HIL_TEMPLATE
HIL<LPN>::HIL(ObjectData &o) : Object(0) {}

HIL_TEMPLATE
HIL<LPN>::~HIL() {}

HIL_TEMPLATE
void HIL<LPN>::readPages(LPN offset, LPN length, uint8_t *buffer, Event eid,
                         EventContext context) {
  // TODO: bypass command to ICL
}

HIL_TEMPLATE
void HIL<LPN>::writePages(LPN offset, LPN length, uint8_t *buffer,
                          std::pair<uint32_t, uint32_t> unwritten, Event eid,
                          EventContext context = EventContext()) {
  // TODO: bypass command to ICL
}

HIL_TEMPLATE
void HIL<LPN>::flushCache(LPN offset, LPN length, Event eid,
                          EventContext context = EventContext()) {
  // TODO: bypass command to ICL
}

HIL_TEMPLATE
void HIL<LPN>::trimPages(LPN offset, LPN length, Event eid,
                         EventContext context = EventContext()) {
  // TODO: bypass command to ICL
}

HIL_TEMPLATE
void HIL<LPN>::formatPages(LPN offset, LPN length, FormatOption option,
                           Event eid, EventContext context = EventContext()) {
  // TODO: bypass command to ICL
}

HIL_TEMPLATE
LPN HIL<LPN>::getPageUsage() {
  // TODO: bypass command to ICL
}

HIL_TEMPLATE
LPN HIL<LPN>::getTotalPages() {
  // TODO: bypass command to ICL
}

HIL_TEMPLATE
uint64_t HIL<LPN>::getLPNSize() {
  // TODO: bypass command to ICL
}

}  // namespace SimpleSSD::HIL

#endif
