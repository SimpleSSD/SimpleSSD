// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "sim/simplessd.hh"

namespace SimpleSSD {

//! SimpleSSD constructor
SimpleSSD::SimpleSSD() : inited(false), config(nullptr), engine(nullptr) {}

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
void SimpleSSD::openStream(std::ofstream &os, std::string &prefix,
                           std::string &path) noexcept {
  // Check path is FILE_STDOUT or FILE_STDERR
  if (path.compare(FILE_STDOUT) == 0) {
    std::streambuf *sb = std::cout.rdbuf();
    os = std::ofstream(sb);
  }
  else if (path.compare(FILE_STDERR) == 0) {
    std::streambuf *sb = std::cerr.rdbuf();
    os = std::ofstream(sb);
  }
  else {
    std::string filepath = prefix;

    joinPath(filepath, path);

    os.open(filepath);

    if (!os.is_open()) {
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
 * \return Initialization result.
 */
bool SimpleSSD::init(Config *c, engine *e) noexcept {
  config = c;
  engine = e;

  // Open file streams
  auto prefix =
      config->readString(Config::Section::Sim, SimConfig::OutputDirectory);
  auto outpath =
      config->readString(Config::Section::Sim, SimConfig::OutputFile);
  auto errpath = config->readString(Config::Section::Sim, SimConfig::ErrorFile);
  auto debugpath =
      config->readString(Config::Section::Sim, SimConfig::DebugFile);

  openStream(outfile, prefix, outpath);
  openStream(errfile, prefix, errpath);
  openStream(debugfile, prefix, debugpath);

  // Initialize log system
  log.init(outfile, errfile, debugfile);

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
  }

  inited = false;
}

}  // namespace SimpleSSD
