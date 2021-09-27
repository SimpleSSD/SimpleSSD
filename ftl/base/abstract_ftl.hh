// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_BASE_ABSTRACT_FTL_HH__
#define __SIMPLESSD_FTL_BASE_ABSTRACT_FTL_HH__

#include "fil/fil.hh"
#include "ftl/def.hh"
#include "ftl/object.hh"
#include "sim/object.hh"

namespace SimpleSSD::FTL {

class FTL;

class AbstractFTL : public Object {
 private:
  FTL *pFTL;

 protected:
  FTLObjectData &ftlobject;

  FIL::FIL *pFIL;

  /**
   * \brief Completion callback of FTL::Request
   */
  void completeRequest(Request *);

 public:
  AbstractFTL(ObjectData &, FTLObjectData &, FTL *, FIL::FIL *);
  virtual ~AbstractFTL();

  /**
   * \brief Get FTL::Request from tag id
   *
   * This functino is public because of ReadModifyWriteContext
   *
   * \return Pointer to FTL::Request
   */
  Request *getRequest(uint64_t);

  /**
   * \brief FTL initialization function
   *
   * Immediately call AbstractFTL::initialize() when you override this function.
   */
  virtual void initialize();

  /**
   * \brief Do Read I/O
   *
   * Handle Read I/O Request. FTL should translate LPN to PPN, submit NAND I/O
   * to FTL.
   *
   * \param[in] req Pointer to FTL::Request
   */
  virtual void read(Request *req) = 0;

  /**
   * \brief Do Write I/O
   *
   * Handle Write I/O Request. FTL should translate LPN to PPN, submit NAND I/O
   * to FTL.
   *
   * Caution:
   *  - You must care about read-modify-write operation.
   *  - You must stop handling write requests when there are no free blocks.
   *  - You must care about small-sized sequential write request when mapping
   *    granularity is larger than physical page size.
   *
   * \param[in] req Pointer to FTL::Request
   * \return True when request is handled. False when stalled.
   */
  virtual bool write(Request *req) = 0;

  /**
   * \brief Do Trim/Format
   *
   * Remove existing mapping from the table.
   *
   * TODO: Maybe refined -- currently no TRIM implementation
   *
   * \param[in] req Pointer to FTL::Request
   */
  virtual void invalidate(Request *req) = 0;

  /**
   * \brief Restart write stalled requests
   *
   * Restart stalled write request. If GC module reclaims block, it will call
   * this function to restart some write requests. You must stop submitting
   * when AbstractAllocator::checkForegroundGCThreshold returns true.
   */
  virtual void restartStalledRequests() = 0;

  // In initialize phase of mapping, they may want to write spare area
  void writeSpare(PPN ppn, uint8_t *buffer, uint64_t size) {
    pFIL->writeSpare(ppn, buffer, size);
  }
};

}  // namespace SimpleSSD::FTL

#endif
