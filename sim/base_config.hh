// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIM_BASE_CONFIG_HH__
#define __SIM_BASE_CONFIG_HH__

#include <cinttypes>
#include <string>

#include "lib/pugixml/src/pugixml.hpp"

namespace SimpleSSD {

#define CONFIG_NODE_NAME "simplessd"
#define CONFIG_SECTION_NAME "section"
#define CONFIG_KEY_NAME "config"
#define CONFIG_ATTRIBUTE "name"

// Use following macros only in BaseConfig (and its child class)

#define LOAD_NAME(node, attr, out, op, def)                                    \
  {                                                                            \
    if (strcmp((node).attribute(CONFIG_ATTRIBUTE).value(), attr) == 0) {       \
      out = (int64_t)(node).text().op(def);                                    \
    }                                                                          \
  }

#define LOAD_NAME_INT(node, attr, out, def)                                    \
  {                                                                            \
    if (strcmp((node).attribute(CONFIG_ATTRIBUTE).value(), attr) == 0) {       \
      bool flag = false;                                                       \
      auto str = (node).child_value();                                         \
                                                                               \
      out = convertInt(str, &flag);                                            \
                                                                               \
      if (!flag) {                                                             \
        out = def;                                                             \
      }                                                                        \
    }                                                                          \
  }

#define LOAD_NAME_UINT(node, attr, out, def)                                   \
  {                                                                            \
    if (strcmp((node).attribute(CONFIG_ATTRIBUTE).value(), attr) == 0) {       \
      bool flag = false;                                                       \
      auto str = (node).child_value();                                         \
                                                                               \
      out = convertUint(str, &flag);                                           \
                                                                               \
      if (!flag) {                                                             \
        out = def;                                                             \
      }                                                                        \
    }                                                                          \
  }

#define LOAD_NAME_TIME(node, attr, out, def)                                   \
  {                                                                            \
    if (strcmp((node).attribute(CONFIG_ATTRIBUTE).value(), attr) == 0) {       \
      bool flag = false;                                                       \
      auto str = (node).child_value();                                         \
                                                                               \
      out = convertTime(str, &flag);                                           \
                                                                               \
      if (!flag) {                                                             \
        out = def;                                                             \
      }                                                                        \
    }                                                                          \
  }

#define LOAD_NAME_BOOLEAN(node, attr, out, def)                                \
  LOAD_NAME(node, attr, out, as_bool, def)

#define LOAD_NAME_TEXT(node, attr, out, def)                                   \
  LOAD_NAME(node, attr, out, as_string, def)

#define LOAD_NAME_FLOAT(node, attr, out, def)                                  \
  LOAD_NAME(node, attr, out, as_float, def)

#define STORE_NAME(section, attr, type, in)                                    \
  {                                                                            \
    auto child = (section).append_child(CONFIG_KEY_NAME);                      \
                                                                               \
    if (child) {                                                               \
      child.append_attribute(CONFIG_ATTRIBUTE).set_value(attr);                \
      child.text().set((type)in);                                              \
    }                                                                          \
  }

#define STORE_NAME_INT(section, attr, in)                                      \
  STORE_NAME(section, attr, long long, in)

#define STORE_NAME_UINT(section, attr, in)                                     \
  STORE_NAME(section, attr, unsigned long long, in)

#define STORE_NAME_TIME STORE_NAME_UINT

#define STORE_NAME_BOOLEAN(section, attr, in)                                  \
  STORE_NAME(section, attr, bool, in)

#define STORE_NAME_TEXT(section, attr, in)                                     \
  STORE_NAME(section, attr, const char *, in)

#define STORE_NAME_FLOAT(section, attr, in) STORE_NAME(section, attr, float, in)

#define STORE_SECTION(parent, name, section)                                   \
  {                                                                            \
    section = parent.append_child(CONFIG_SECTION_NAME);                        \
    section.append_attribute(CONFIG_ATTRIBUTE).set_value(name);                \
  }

/**
 * \brief BaseConfig object declaration
 *
 * Abstract class for configuration object. You must inherit this class to
 * create your own configuration section in xml file.
 */
class BaseConfig {
 protected:
  int64_t convertInt(const char *, bool * = nullptr);
  uint64_t convertUint(const char *, bool * = nullptr);
  uint64_t convertTime(const char *, bool * = nullptr);

 public:
  BaseConfig();
  virtual ~BaseConfig();

  virtual const char *getSectionName() = 0;

  virtual void loadFrom(pugi::xml_node &) = 0;
  virtual void storeTo(pugi::xml_node &) = 0;
  virtual void update() {}

  virtual int64_t readInt(uint32_t) { return 0; }
  virtual uint64_t readUint(uint32_t) { return 0; }
  virtual float readFloat(uint32_t) { return 0.f; }
  virtual std::string readString(uint32_t) { return std::string(); }
  virtual bool readBoolean(uint32_t) { return false; }

  virtual bool writeInt(uint32_t, int64_t) { return false; }
  virtual bool writeUint(uint32_t, uint64_t) { return false; }
  virtual bool writeFloat(uint32_t, float) { return false; }
  virtual bool writeString(uint32_t, std::string &) { return false; }
  virtual bool writeBoolean(uint32_t, bool) { return false; }
};

}  // namespace SimpleSSD

#endif
