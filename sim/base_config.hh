// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#pragma once

#ifndef __SIMPLESSD_SIM_BASE_CONFIG_HH__
#define __SIMPLESSD_SIM_BASE_CONFIG_HH__

#include <cinttypes>
#include <cstring>
#include <string>

#include "lib/pugixml/src/pugixml.hpp"

namespace SimpleSSD {

#undef panic_if
#undef warn_if

#define CONFIG_NODE_NAME "simplessd"
#define CONFIG_SECTION_NAME "section"
#define CONFIG_KEY_NAME "config"
#define CONFIG_ATTRIBUTE "name"

// Use following macros only in BaseConfig (and its child class)

#define LOAD_NAME(node, attr, out, op)                                         \
  {                                                                            \
    if (strcmp((node).attribute(CONFIG_ATTRIBUTE).value(), attr) == 0 &&       \
        isKey(node)) {                                                         \
      out = (node).text().op(out);                                             \
    }                                                                          \
  }

#define LOAD_NAME_INT(node, attr, out)                                         \
  {                                                                            \
    if (strcmp((node).attribute(CONFIG_ATTRIBUTE).value(), attr) == 0 &&       \
        isKey(node)) {                                                         \
      bool _flag = false;                                                      \
      auto _str = (node).child_value();                                        \
      auto _def = out;                                                         \
                                                                               \
      out = convertInt(_str, &_flag);                                          \
                                                                               \
      if (!_flag) {                                                            \
        out = _def;                                                            \
      }                                                                        \
    }                                                                          \
  }

#define LOAD_NAME_UINT_TYPE(node, attr, type, out)                             \
  {                                                                            \
    if (strcmp((node).attribute(CONFIG_ATTRIBUTE).value(), attr) == 0 &&       \
        isKey(node)) {                                                         \
      bool _flag = false;                                                      \
      auto _str = (node).child_value();                                        \
      auto _def = out;                                                         \
                                                                               \
      out = (type)convertUint(_str, &_flag);                                   \
                                                                               \
      if (!_flag) {                                                            \
        out = _def;                                                            \
      }                                                                        \
    }                                                                          \
  }

#define LOAD_NAME_UINT(node, attr, out)                                        \
  LOAD_NAME_UINT_TYPE(node, attr, uint64_t, out)

#define LOAD_NAME_TIME(node, attr, out)                                        \
  {                                                                            \
    if (strcmp((node).attribute(CONFIG_ATTRIBUTE).value(), attr) == 0 &&       \
        isKey(node)) {                                                         \
      bool _flag = false;                                                      \
      auto _str = (node).child_value();                                        \
      auto _def = out;                                                         \
                                                                               \
      out = convertTime(_str, &_flag);                                         \
                                                                               \
      if (!_flag) {                                                            \
        out = _def;                                                            \
      }                                                                        \
    }                                                                          \
  }

#define LOAD_NAME_BOOLEAN(node, attr, out) LOAD_NAME(node, attr, out, as_bool)

#define LOAD_NAME_TEXT(node, attr, out) LOAD_NAME(node, attr, out, as_string)

#define LOAD_NAME_STRING(node, attr, out)                                      \
  {                                                                            \
    if (strcmp((node).attribute(CONFIG_ATTRIBUTE).value(), attr) == 0 &&       \
        isKey(node)) {                                                         \
      out = (node).text().as_string(out.c_str());                              \
    }                                                                          \
  }

#define LOAD_NAME_FLOAT(node, attr, out) LOAD_NAME(node, attr, out, as_float)

#define STORE_NAME(section, attr, type, in)                                    \
  {                                                                            \
    auto _child = (section).append_child(CONFIG_KEY_NAME);                     \
                                                                               \
    if (_child) {                                                              \
      _child.append_attribute(CONFIG_ATTRIBUTE).set_value(attr);               \
      _child.text().set((type)in);                                             \
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

#define STORE_NAME_STRING(section, attr, in)                                   \
  STORE_NAME(section, attr, const char *, in.c_str())

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

  static bool isSection(pugi::xml_node &);
  static bool isKey(pugi::xml_node &);

  static void panic_if(bool, const char *, ...);
  static void warn_if(bool, const char *, ...);

 public:
  BaseConfig();

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
