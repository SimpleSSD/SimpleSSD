// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "sim/simplessd.hh"

#include <iostream>

#include "hil/nvme/subsystem.hh"
#include "mem/dram/gem5.hh"
#include "mem/dram/simple.hh"
#include "mem/sram/sram.hh"
#include "sim/version.hh"
#include "util/path.hh"

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
    os = &std::cout;
  }
  else if (path.compare(FILE_STDERR) == 0) {
    os = &std::cerr;
  }
  else {
    std::string filepath = Path::joinPath(prefix.c_str(), path.c_str());

    os = new std::ofstream(filepath);

    if (!((std::ofstream *)os)->is_open()) {
      // Log system not initialized yet
      std::cerr << "panic: Failed to open file: " << filepath << std::endl;
      abort();
    }
  }

  return os;
}

void SimpleSSD::closeStream(std::ostream *os) noexcept {
  if (os == &std::cout || os == &std::cerr) {
    // This output stream is standard I/O
    return;
  }

  if (auto ofs = dynamic_cast<std::ofstream *>(os)) {
    ofs->close();
  }

  delete os;
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

  // Initialize hardware
  object.cpu = new CPU::CPU(e, c, &log);

  log.init(object.cpu, outfile, errfile, debugfile);

  object.sram = new Memory::SRAM::SRAM(object);

  switch ((Memory::Config::Model)c->readUint(Section::Memory,
                                             Memory::Config::Key::DRAMModel)) {
    case Memory::Config::Model::Simple:
      object.dram = new Memory::DRAM::SimpleDRAM(object);
      break;
    case Memory::Config::Model::Gem5:
      object.dram = new Memory::DRAM::TimingDRAM(object);
      break;
    default:
      std::cerr << "Invalid DRAM model selected." << std::endl;

      abort();
  }

  // Initialize objects
  switch (mode) {
    case Config::Mode::NVMe:
      subsystem = new HIL::NVMe::Subsystem(object);
      break;
    default:
      std::cerr << "Invalid controller selected." << std::endl;

      abort();
  }

  // Initialize Subsystem
  subsystem->init();

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
    delete subsystem;

    // Deinitialize hardware
    delete object.sram;
    delete object.dram;

    log.deinit();

    delete object.cpu;

    // Close files
    closeStream(outfile);
    closeStream(errfile);
    closeStream(debugfile);

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

ObjectData &SimpleSSD::getObject() {
  return object;
}

void SimpleSSD::getStatList(std::vector<Stat> &list,
                            std::string prefix) noexcept {
  subsystem->getStatList(list, prefix);
  object.cpu->getStatList(list, prefix + "cpu.");
  object.dram->getStatList(list, prefix + "dram.");
  object.sram->getStatList(list, prefix + "sram.");
}

void SimpleSSD::getStatValues(std::vector<double> &values) noexcept {
  subsystem->getStatValues(values);
  object.cpu->getStatValues(values);
  object.dram->getStatValues(values);
  object.sram->getStatValues(values);
}

void SimpleSSD::resetStatValues() noexcept {
  subsystem->resetStatValues();
  object.cpu->resetStatValues();
  object.dram->resetStatValues();
  object.sram->resetStatValues();
}

void SimpleSSD::createCheckpoint(std::string cpt_dir) const noexcept {
  std::string cpt_file =
      Path::joinPath(cpt_dir.c_str(), SIMPLESSD_CHECKPOINT_NAME);
  std::string cpt_config =
      Path::joinPath(cpt_dir.c_str(), SIMPLESSD_CHECKPOINT_CONFIG);

  // Try to open file at path
  std::ofstream file(cpt_file, std::ios::binary);

  if (!file.is_open()) {
    std::cerr << "Failed to open checkpoint file: " << cpt_file << std::endl;

    abort();
  }

  // Checkpointing current configuration
  object.config->save(cpt_config);

  // Checkpointing this
  std::string version(SIMPLESSD_VERSION);
  uint64_t size = version.size();

  BACKUP_SCALAR(file, size);
  BACKUP_BLOB(file, version.c_str(), size);

  // Backup hardware first
  object.cpu->createCheckpoint(file);
  object.dram->createCheckpoint(file);
  object.sram->createCheckpoint(file);

  // All simulation objects
  subsystem->createCheckpoint(file);

  file.close();
}

void SimpleSSD::restoreCheckpoint(std::string cpt_dir) noexcept {
  std::string cpt_file =
      Path::joinPath(cpt_file.c_str(), SIMPLESSD_CHECKPOINT_NAME);

  // Try to open file
  std::ifstream file(cpt_file, std::ios::binary);

  if (!file.is_open()) {
    std::cerr << "Failed to open checkpoint file: " << cpt_file << std::endl;

    abort();
  }

  // Restore this
  uint64_t size;
  std::string version;

  RESTORE_SCALAR(file, size);

  version.resize(size);

  RESTORE_BLOB(file, version.c_str(), size);

  if (version.compare(SIMPLESSD_VERSION) != 0) {
    std::cerr << "Version does not match while restore from checkpoint."
              << std::endl;
    std::cerr << " Checkpoint file: " << version << std::endl;
    std::cerr << " Program version: " << SIMPLESSD_VERSION << std::endl;
  }

  // Restore chain begins here
  object.cpu->restoreCheckpoint(file);
  object.dram->restoreCheckpoint(file);
  object.sram->restoreCheckpoint(file);

  subsystem->restoreCheckpoint(file);

  file.close();
}

}  // namespace SimpleSSD
