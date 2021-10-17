// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_FTL_BACKGROUND_MANAGER_ABSTRACT_BACKGROUND_JOB_HH__
#define __SIMPLESSD_FTL_BACKGROUND_MANAGER_ABSTRACT_BACKGROUND_JOB_HH__

#include "fil/fil.hh"
#include "ftl/def.hh"
#include "ftl/object.hh"
#include "sim/object.hh"

namespace SimpleSSD::FTL {

enum class TriggerType : uint32_t {
  ReadMapping,    // Before accessing mapping table
  ReadSubmit,     // After accessing mapping table, before FIL read submission
  ReadComplete,   // After FIL completion
  WriteMapping,   // Before updating mapping table
  WriteSubmit,    // After updating mapping table, before FIL write submission
  WriteComplete,  // After FIL completion

  // TODO: TRIM/Format?

  ForegroundGCRequest,  // This is special -- FTL requires forced GC invocation
                        // Note: Pointer to Request may nullptr
};

class AbstractJob : public Object {
 protected:
  FTLObjectData &ftlobject;

 public:
  AbstractJob(ObjectData &, FTLObjectData &);
  virtual ~AbstractJob();

  /**
   * \brief Initialize abstract job
   *
   * This function will be called after all objects in FTLObjectData have been
   * initialized.
   *
   * \param[in] restore True if restore state from checkpoint
   */
  virtual void initialize(bool restore) = 0;

  /**
   * \brief Query job is running
   *
   * \return true if job is running
   */
  virtual bool isRunning() = 0;

  /**
   * \brief Triggered by user I/O
   *
   * \param when Trigger type.
   * \param req  Request that triggered this event.
   */
  virtual void triggerByUser(TriggerType when, Request *req);

  /**
   * \brief Triggered by SSD idleness
   *
   * \param now      Current simulation tick
   * \param deadline Expected timestamp of next user I/O
   */
  virtual void triggerByIdle(uint64_t now, uint64_t deadline);
};

class AbstractBlockCopyJob : public AbstractJob {
 protected:
  FIL::FIL *const pFIL;

  std::vector<CopyContext> targetBlocks;

  const Parameter *const param;
  const uint64_t bufferBaseAddress;
  const uint32_t superpage;
  const uint32_t pageSize;

  const Log::DebugID logid;
  const char *const logprefix;

  //!< Helper function to calculate offset of buffer
  inline uint64_t makeBufferAddress(uint32_t blockIndex,
                                    uint32_t superpageIndex) {
    return bufferBaseAddress +
           (blockIndex * superpage + superpageIndex) * pageSize;
  }

  /*
   * State machine for block copy.
   *
   * Read Page -> Update mapping table -> Write Page -.
   *   |    `-------------------------------------<---'
   *   `--------> Erase block ---> Done
   */

  /**
   * \brief Perform page read or erase
   *
   * This event is intended to be completion handler of
   * AbstractAllocator::getVictimBlock.
   *
   * If we copied all valid pages in CopyContext:
   *   Issue Erase request to FIL with completion handler of eventEraseDone.
   * Else:
   *   Issue Read request to FIL with completion handler of eventUpdateMapping.
   *
   * \param[in] now         Current simulation tick
   * \param[in] blockIndex  Index of copy context
   */
  Event eventReadPage;
  virtual void readPage(uint64_t now, uint32_t blockIndex);

  /**
   * \brief Perform mapping table update
   *
   * Ask mapping class to get where to write new page.
   * Issue writeMapping with completion handler of eventWritePage.
   *
   * \param[in] now         Current simulation tick
   * \param[in] blockIndex  Index of copy context
   */
  Event eventUpdateMapping;
  virtual void updateMapping(uint64_t now, uint32_t blockIndex);

  /**
   * \brief Perform write
   *
   * Issue Program request to FIL with completion handler of eventWriteDone.
   *
   * \param[in] now         Current simulation tick
   * \param[in] blockIndex  Index of copy context
   */
  Event eventWritePage;
  virtual void writePage(uint64_t now, uint32_t blockIndex);

  /**
   * \brief Completion handler of write
   *
   * This state handles multiple program requests caused by superpage
   * configuration. If all program requests are completed, go to eventReadPage.
   *
   * \param[in] now         Current simulation tick
   * \param[in] blockIndex  Index of copy context
   */
  Event eventWriteDone;
  virtual void writeDone(uint64_t now, uint32_t blockIndex);

  /**
   * \brief Completion handler of erase
   *
   * This state handles multiple erase requests caused by superpage
   * configuration. If all erase requests are completed, reclaim block by
   * calling AbstractAllocator::reclaimBlock.
   *
   * \param[in] now         Current simulation tick
   * \param[in] blockIndex  Index of copy context
   */
  Event eventEraseDone;
  virtual void eraseDone(uint64_t now, uint32_t blockIndex);

  /**
   * \brief Completion handler of copy operation
   *
   * This state clears CopyContext and checks termination condition.
   *
   * \param[in] now         Current simulation tick
   * \param[in] blockIndex  Index of copy context
   */
  Event eventDone;
  virtual void done(uint64_t now, uint32_t blockIndex) = 0;

  /**
   * \brief Configure abstract job
   *
   * \param logID Debug log Id to use
   * \param log   Log prefix to use
   * \param obj   Object name to use
   * \param size  Size of targetBlocks
   */
  void configure(const Log::DebugID logID, const char *log, const char *obj,
                 const uint32_t size);

 public:
  AbstractBlockCopyJob(ObjectData &, FTLObjectData &, FIL::FIL *);
  virtual ~AbstractBlockCopyJob();

  void createCheckpoint(std::ostream &) const noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::FTL

#endif
