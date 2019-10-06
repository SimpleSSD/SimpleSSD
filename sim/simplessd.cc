// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "sim/simplessd.hh"

#include <iostream>

#include "hil/nvme/subsystem.hh"

#define SIMPLESSD_CHECKPOINT_NAME "simplessd.bin"
#define SIMPLESSD_CHECKPOINT_CONFIG "config.xml"

namespace SimpleSSD {

//! SimpleSSD constructor
SimpleSSD::SimpleSSD()
    : inited(false),
      subsystem(nullptr),
      outfile(nullptr),
      errfile(nullptr),
      debugfile(nullptr) {}

//! SimpleSSD destructor
SimpleSSD::~SimpleSSD() {
  if (inited) {
    deinit();
  }
}

/**
 * \brief Join path
 *
 * Join two path strings
 *
 * \param[in,out] prefix First path string
 * \param[in]     path   Second path string
 */
void SimpleSSD::joinPath(std::string &prefix, std::string path) noexcept {
  if (prefix.back() != '/' && prefix.back() != '\\') {
    prefix.push_back('/');
  }

  if (path[0] == '/' || path[0] == '\\') {
    prefix.push_back('.');
  }

  prefix += path;
}

/**
 * \brief Open output file stream
 *
 * Open output stream specified in path argument.
 * If path is FILE_STDOUT or FILE_STDERR, it opens std::cout or std::cerr.
 *
 * \param[out] os     Output file stream
 * \param[in]  prefix First path string (directory path)
 * \param[in]  path   Second path string (file name)
 */
std::ostream *SimpleSSD::openStream(std::string &prefix,
                                    std::string &path) noexcept {
  std::ostream *os = nullptr;

  // Check path is FILE_STDOUT or FILE_STDERR
  if (path.compare(FILE_STDOUT) == 0) {
    std::streambuf *sb = std::cout.rdbuf();
    os = new std::ostream(sb);
  }
  else if (path.compare(FILE_STDERR) == 0) {
    std::streambuf *sb = std::cerr.rdbuf();

    os = new std::ostream(sb);
  }
  else {
    std::string filepath = prefix;

    joinPath(filepath, path);

    os = new std::ofstream(filepath);

    if (!((std::ofstream *)os)->is_open()) {
      // Log system not initialized yet
      std::cerr << "panic: Failed to open file: " << filepath << std::endl;
      abort();
    }
  }

  return os;
}

/**
 * \brief Initialize SimpleSSD
 *
 * Initialize all components of SSD. You must call this function before
 * call any methods of SimpleSSD object.
 *
 * \param[in]  c  SimpleSSD::Config object.
 * \param[in]  e  SimpleSSD::Engine object.
 * \param[in]  i  SimpleSSD::Interface object.
 * \return Initialization result.
 */
bool SimpleSSD::init(Engine *e, ConfigReader *c) noexcept {
  object.engine = e;
  object.config = c;
  object.log = &log;

  // Open file streams
  auto prefix = c->readString(Section::Simulation, Config::OutputDirectory);
  auto outpath = c->readString(Section::Simulation, Config::OutputFile);
  auto errpath = c->readString(Section::Simulation, Config::ErrorFile);
  auto debugpath = c->readString(Section::Simulation, Config::DebugFile);
  auto mode =
      (Config::Mode)c->readUint(Section::Simulation, Config::Controller);

  outfile = openStream(prefix, outpath);
  errfile = openStream(prefix, errpath);
  debugfile = openStream(prefix, debugpath);

  // Initialize log system
  log.init(e, outfile, errfile, debugfile);

  // Initialize objects
  switch (mode) {
    case Config::Mode::NVMe:
      subsystem = new HIL::NVMe::Subsystem(object);
      break;
    default:
      std::cerr << "Invalid controller selected." << std::endl;

      abort();
  }

  // Create Subsystem

  inited = true;

  return inited;
}

/**
 * \brief Deinitialize SimpleSSD
 *
 * Release all resources allocated for this object.
 */
void SimpleSSD::deinit() noexcept {
  if (inited) {
    // Delete objects

    // Deinitialize log system
    log.deinit();

    // outfile, errfile and debugfile are closed by Log::deinit()
    delete outfile;
    delete errfile;
    delete debugfile;

    outfile = nullptr;
    errfile = nullptr;
    debugfile = nullptr;
  }

  inited = false;
}

ControllerID SimpleSSD::createController(Interface *i) {
  return subsystem->createController(i);
}

AbstractController *SimpleSSD::getController(ControllerID cid) {
  if (inited) {
    return subsystem->getController(cid);
  }

  return nullptr;
}

void SimpleSSD::getStatList(std::vector<Object::Stat> &, std::string) noexcept {
}

void SimpleSSD::getStatValues(std::vector<double> &) noexcept {}

void SimpleSSD::resetStatValues() noexcept {}

void SimpleSSD::createCheckpoint(std::string cpt_dir) noexcept {
  std::string cpt_file(cpt_dir);
  std::string cpt_config(cpt_dir);

  joinPath(cpt_file, SIMPLESSD_CHECKPOINT_NAME);
  joinPath(cpt_config, SIMPLESSD_CHECKPOINT_CONFIG);

  // Try to open file at path
  std::ofstream file(cpt_file, std::ios::binary);

  if (!file.is_open()) {
    std::cerr << "Failed to open checkpoint file: " << cpt_file << std::endl;

    abort();
  }

  // Checkpointing current configuration
  object.config->writeString(Section::Simulation, Config::Key::CheckpointFile,
                             cpt_file);
  object.config->save(cpt_config);

  // Checkpointing this
  // TODO: WRITE VERSION HERE!
  object.engine->createCheckpoint(file);

  // Checkpoint chain begins here
  subsystem->createCheckpoint(file);

  file.close();
}

void SimpleSSD::restoreCheckpoint(Engine *e, ConfigReader *c) noexcept {
  // First, create all objects
  init(e, c);

  auto cpt_file =
      c->readString(Section::Simulation, Config::Key::CheckpointFile);

  // Try to open file
  std::ifstream file(cpt_file, std::ios::binary);

  if (!file.is_open()) {
    std::cerr << "Failed to open checkpoint file: " << cpt_file << std::endl;

    abort();
  }

  // Restore this
  // TODO: CHECK VERSION HERE!
  object.engine->restoreCheckpoint(file);

  // Restore chain begins here
  subsystem->restoreCheckpoint(file);

  file.close();
}

}  // namespace SimpleSSD
