// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "hil/hil.hh"

#ifndef HIL_TEMPLATE
#define HIL_TEMPLATE                                                           \
  template <class LPN, std::enable_if_t<std::is_unsigned_v<LPN>, LPN> T>

namespace SimpleSSD::HIL {

HIL_TEMPLATE
HIL<LPN, T>::HIL(ObjectData &o) : Object(o) {}

HIL_TEMPLATE
HIL<LPN, T>::~HIL() {}

HIL_TEMPLATE
void HIL<LPN, T>::readPages(LPN offset, LPN length, uint8_t *buffer,
                            Event eid) {
  // TODO: bypass command to ICL
}

HIL_TEMPLATE
void HIL<LPN, T>::writePages(LPN offset, LPN length, uint8_t *buffer,
                             std::pair<uint32_t, uint32_t> unwritten,
                             Event eid) {
  // TODO: bypass command to ICL
}

HIL_TEMPLATE
void HIL<LPN, T>::flushCache(LPN offset, LPN length, Event eid) {
  // TODO: bypass command to ICL
}

HIL_TEMPLATE
void HIL<LPN, T>::trimPages(LPN offset, LPN length, Event eid) {
  // TODO: bypass command to ICL
}

HIL_TEMPLATE
void HIL<LPN, T>::formatPages(LPN offset, LPN length, FormatOption option,
                              Event eid) {
  // TODO: bypass command to ICL
}

HIL_TEMPLATE
LPN HIL<LPN, T>::getPageUsage() {
  // TODO: bypass command to ICL
  return (LPN)0;
}

HIL_TEMPLATE
LPN HIL<LPN, T>::getTotalPages() {
  // TODO: bypass command to ICL
  return (LPN)0;
}

HIL_TEMPLATE
uint64_t HIL<LPN, T>::getLPNSize() {
  // TODO: bypass command to ICL
  return 0;
}

}  // namespace SimpleSSD::HIL

#endif
