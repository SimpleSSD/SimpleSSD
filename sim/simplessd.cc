// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "sim/simplessd.hh"

#include <iostream>

#define SIMPLESSD_CHECKPOINT_NAME "simplessd.bin"
#define SIMPLESSD_CHECKPOINT_CONFIG "config.xml"

namespace SimpleSSD {

//! SimpleSSD constructor
SimpleSSD::SimpleSSD()
    : inited(false),
      config(nullptr),
      engine(nullptr),
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
void SimpleSSD::openStream(std::ostream *os, std::string &prefix,
                           std::string &path) noexcept {
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
  engine = e;
  config = c;

  // Open file streams
  auto prefix =
      config->readString(Section::Simulation, Config::OutputDirectory);
  auto outpath = config->readString(Section::Simulation, Config::OutputFile);
  auto errpath = config->readString(Section::Simulation, Config::ErrorFile);
  auto debugpath = config->readString(Section::Simulation, Config::DebugFile);
  auto mode =
      (Config::Mode)config->readUint(Section::Simulation, Config::Controller);

  openStream(outfile, prefix, outpath);
  openStream(errfile, prefix, errpath);
  openStream(debugfile, prefix, debugpath);

  // Initialize log system
  log.init(engine, outfile, errfile, debugfile);

  // Initialize objects
  switch (mode) {
    case Config::Mode::NVMe:
      // pHIL = ;
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

void SimpleSSD::destroyController(ControllerID cid) {
  subsystem->destroyController(cid);
}

AbstractController *SimpleSSD::getController(ControllerID cid) {
  if (inited) {
    return subsystem->getController(cid);
  }

  return nullptr;
}

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
  config->writeString(Section::Simulation, Config::Key::CheckpointFile,
                      cpt_file);
  config->save(cpt_config);

  // Checkpointing this
  // TODO: WRITE VERSION HERE!
  engine->createCheckpoint(file);

  // Checkpoint chain begins here
  subsystem->createCheckpoint(file);

  file.close();
}

void SimpleSSD::restoreCheckpoint(ConfigReader *) noexcept {
  auto cpt_file =
      config->readString(Section::Simulation, Config::Key::CheckpointFile);

  // Try to open file
  std::ifstream file(cpt_file, std::ios::binary);

  if (!file.is_open()) {
    std::cerr << "Failed to open checkpoint file: " << cpt_file << std::endl;

    abort();
  }

  // Restore this
  // TODO: CHECK VERSION HERE!
  engine->restoreCheckpoint(file);

  // Restore chain begins here
  subsystem->restoreCheckpoint(file);

  file.close();
}

}  // namespace SimpleSSD
