// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "sim/simplessd.hh"

#include <iostream>

namespace SimpleSSD {

//! SimpleSSD constructor
SimpleSSD::SimpleSSD()
    : inited(false),
      config(nullptr),
      engine(nullptr),
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
void SimpleSSD::joinPath(std::string &prefix, std::string &path) noexcept {
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
bool SimpleSSD::init(Engine *e, ConfigReader *c, Interface *i) noexcept {
  engine = e;
  config = c;
  interface = i;

  // Open file streams
  auto prefix =
      config->readString(Section::Simulation, Config::OutputDirectory);
  auto outpath = config->readString(Section::Simulation, Config::OutputFile);
  auto errpath = config->readString(Section::Simulation, Config::ErrorFile);
  auto debugpath = config->readString(Section::Simulation, Config::DebugFile);

  openStream(outfile, prefix, outpath);
  openStream(errfile, prefix, errpath);
  openStream(debugfile, prefix, debugpath);

  // Initialize log system
  log.init(engine, outfile, errfile, debugfile);

  // Initialize objects
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

/**
 * \brief Read register
 *
 * Read controller register of SSD.
 *
 * \param[in]  offset   Offset of controller register
 * \param[in]  length   Length to read
 * \param[out] buffer   Buffer
 * \param[in]  eid      Event ID
 * \param[in]  context  User data
 */
void SimpleSSD::read(uint64_t offset, uint64_t length, uint8_t *buffer,
                     Event eid, void *context) noexcept {
  uint64_t latency = read(offset, length, buffer);

  engine->schedule(eid, engine->getTick() + latency, context);
}

/**
 * \brief Read register
 *
 * Simulator can use this simple version of read function when Event is not
 * necessary.
 */
uint64_t SimpleSSD::read(uint64_t offset, uint64_t length,
                         uint8_t *buffer) noexcept {
  // TODO: Return from HIL's read function

  return 0;
}

/**
 * \brief Write register
 *
 * Write controller register of SSD.
 *
 * \param[in] offset  Offset of controller register
 * \param[in] length  Length to write
 * \param[in] buffer  Data to write
 * \param[in] eid     Event ID
 * \param[in] context User data
 */
void SimpleSSD::write(uint64_t offset, uint64_t length, uint8_t *buffer,
                      Event eid, void *context) noexcept {
  uint64_t latency = write(offset, length, buffer);

  engine->schedule(eid, engine->getTick() + latency, context);
}

/**
 * \brief Write register
 *
 * Simulator can use this simple version of write function when Event is not
 * necessary.
 */
uint64_t SimpleSSD::write(uint64_t offset, uint64_t length,
                          uint8_t *buffer) noexcept {
  // TODO: Return from HIL's write function

  return 0;
}

}  // namespace SimpleSSD
