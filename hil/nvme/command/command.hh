// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NVME_COMMAND_COMMAND_HH__
#define __SIMPLESSD_HIL_NVME_COMMAND_COMMAND_HH__

// Admin commands
#include "hil/nvme/command/abort.hh"
#include "hil/nvme/command/async_event_request.hh"
#include "hil/nvme/command/create_cq.hh"
#include "hil/nvme/command/create_sq.hh"
#include "hil/nvme/command/delete_cq.hh"
#include "hil/nvme/command/delete_sq.hh"
#include "hil/nvme/command/format_nvm.hh"
#include "hil/nvme/command/get_feature.hh"
#include "hil/nvme/command/get_log_page.hh"
#include "hil/nvme/command/identify.hh"
#include "hil/nvme/command/namespace_attachment.hh"
#include "hil/nvme/command/namespace_management.hh"
#include "hil/nvme/command/set_feature.hh"

// NVM commands
#include "hil/nvme/command/compare.hh"
#include "hil/nvme/command/flush.hh"
#include "hil/nvme/command/read.hh"
#include "hil/nvme/command/write.hh"

#endif
