#!/bin/bash
WORK_DIR=$(pwd)
PREFIX="hil/nvme/command"

if [ ! -d $WORK_DIR"/"$PREFIX ]
then
  echo "Failed to detect NVMe command directory."
  echo " Please run script in SimpleSSD root directory (where CMakeLists.txt exists)."

  exit 1
fi

if [ "$#" -ne 2 ]
then
  echo "Usage : "$0" <filename without extension> <class name>"
  echo " Specify snake_case filename and CamelCase class name."

  exit 1
fi

GIT_NAME=$(git config --get user.name)
GIT_EMAIL=$(git config --get user.email)

if [ $? -ne 0 ]
then
  echo "Failed to query user information"

  exit 1
fi

AUTHOR_STRING=$GIT_NAME" <"$GIT_EMAIL">"
echo "Using Author: "$AUTHOR_STRING

if [[ ! $1 =~ ^[a-z_]+$ ]]
then
  echo "Invalid filename \""$1"\""

  exit 1
fi

if [[ ! $2 =~ ^[a-zA-Z]+$ ]]
then
  echo "Invalid class name \""$2"\""

  exit 1
fi

HEADER_FILE=$1".hh"
SOURCE_FILE=$1".cc"
UPPER_FILENAME=${1^^}
CLASS_NAME=$2

cat > $PREFIX"/"$HEADER_FILE << EOF
// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: ${AUTHOR_STRING}
 */

#pragma once

#ifndef __SIMPLESSD_HIL_NVME_COMMAND_${UPPER_FILENAME}_HH__
#define __SIMPLESSD_HIL_NVME_COMMAND_${UPPER_FILENAME}_HH__

#include "hil/nvme/command/abstract_command.hh"

namespace SimpleSSD::HIL::NVMe {

class ${CLASS_NAME} : public Command {
 private:

 public:
  ${CLASS_NAME}(ObjectData &, Subsystem *, ControllerData *);
  ~${CLASS_NAME}();

  void setRequest(SQContext *) override;

  void getStatList(std::vector<Stat> &, std::string) noexcept override;
  void getStatValues(std::vector<double> &) noexcept override;
  void resetStatValues() noexcept override;

  void createCheckpoint(std::ostream &) noexcept override;
  void restoreCheckpoint(std::istream &) noexcept override;
};

}  // namespace SimpleSSD::HIL::NVMe

#endif
EOF

cat > $PREFIX"/"$SOURCE_FILE << EOF
// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: ${AUTHOR_STRING}
 */

#include "hil/nvme/command/${HEADER_FILE}"

#include "hil/nvme/command/internal.hh"

namespace SimpleSSD::HIL::NVMe {

${CLASS_NAME}::${CLASS_NAME}(ObjectData &o, Subsystem *s, ControllerData *c)
    : Command(o, s, c) {}

${CLASS_NAME}::~${CLASS_NAME}() {}

void ${CLASS_NAME}::setRequest(SQContext *req) {}

void ${CLASS_NAME}::getStatList(std::vector<Stat> &, std::string) noexcept {}

void ${CLASS_NAME}::getStatValues(std::vector<double> &) noexcept {}

void ${CLASS_NAME}::resetStatValues() noexcept {}

void ${CLASS_NAME}::createCheckpoint(std::ostream &out) noexcept {
  Command::createCheckpoint(out);
}

void ${CLASS_NAME}::restoreCheckpoint(std::istream &in) noexcept {
  Command::restoreCheckpoint(in);
}

}  // namespace SimpleSSD::HIL::NVMe
EOF
