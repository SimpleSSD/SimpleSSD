// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "util/sorted_map.hh"

#include <array>
#include <catch2/catch.hpp>

TEST_CASE("Sorted map list") {
  using namespace SimpleSSD;

  map_list<uint32_t, uint32_t *> maplist;

  // Input data
  std::array<uint32_t, 8> numbers = {4, 3, 2, 1, 4, 3, 2, 1};

  SECTION("map_list push_back") {
    maplist.push_back(1, &numbers[0]);
    maplist.push_back(2, &numbers[1]);
    maplist.push_back(3, &numbers[2]);
    maplist.push_back(4, &numbers[3]);

    REQUIRE(maplist.size() == 4);

    REQUIRE(*(maplist.find(1)->second) == numbers[0]);
    REQUIRE(*(maplist.find(2)->second) == numbers[1]);
    REQUIRE(*(maplist.find(3)->second) == numbers[2]);
    REQUIRE(*(maplist.find(4)->second) == numbers[3]);

    REQUIRE(*(maplist.front().second) == numbers[0]);
    REQUIRE(*(maplist.back().second) == numbers[3]);
  }

  SECTION("map_list push_front") {
    maplist.push_front(1, &numbers[0]);
    maplist.push_front(2, &numbers[1]);
    maplist.push_front(3, &numbers[2]);
    maplist.push_front(4, &numbers[3]);

    REQUIRE(maplist.size() == 4);

    REQUIRE(*(maplist.find(1)->second) == numbers[0]);
    REQUIRE(*(maplist.find(2)->second) == numbers[1]);
    REQUIRE(*(maplist.find(3)->second) == numbers[2]);
    REQUIRE(*(maplist.find(4)->second) == numbers[3]);

    REQUIRE(*(maplist.front().second) == numbers[3]);
    REQUIRE(*(maplist.back().second) == numbers[0]);
  }

  maplist.push_front(1, &numbers[0]);
  maplist.push_front(2, &numbers[1]);
  maplist.push_front(3, &numbers[2]);
  maplist.push_front(4, &numbers[3]);

  SECTION("map_list pop_front") {
    maplist.pop_front();
    maplist.pop_front();

    REQUIRE(maplist.size() == 2);

    // REQUIRE(maplist.find(4) == maplist.end());
    // REQUIRE(maplist.find(3) == maplist.end());

    REQUIRE(*(maplist.find(2)->second) == numbers[1]);
    REQUIRE(*(maplist.find(1)->second) == numbers[0]);
  }

  SECTION("map_list pop_back") {
    maplist.pop_back();
    maplist.pop_back();

    REQUIRE(maplist.size() == 2);

    // REQUIRE(maplist.find(1) == maplist.end());
    // REQUIRE(maplist.find(2) == maplist.end());

    REQUIRE(*(maplist.find(3)->second) == numbers[2]);
    REQUIRE(*(maplist.find(4)->second) == numbers[3]);
  }

  SECTION("map_list erase") {
    maplist.erase(maplist.find(2));

    REQUIRE(maplist.size() == 3);

    // REQUIRE(maplist.find(2) == maplist.end());

    REQUIRE(*(maplist.find(1)->second) == numbers[0]);
    REQUIRE(*(maplist.find(3)->second) == numbers[2]);
    REQUIRE(*(maplist.find(4)->second) == numbers[3]);
  }

  SECTION("map_list clear") {
    maplist.clear();

    REQUIRE(maplist.size() == 0);
  }
}

TEST_CASE("Sorted map map") {
  using namespace SimpleSSD;

  map_map<uint32_t, uint32_t *> mapmap(
      [](const uint32_t *a, const uint32_t *b) -> bool { return *a < *b; });

  // Input data
  std::array<uint32_t, 8> numbers = {4, 3, 2, 1, 4, 3, 2, 1};

  mapmap.insert(1, &numbers[0]);
  mapmap.insert(2, &numbers[1]);
  mapmap.insert(3, &numbers[2]);
  mapmap.insert(4, &numbers[3]);
  mapmap.insert(10, &numbers[4]);
  mapmap.insert(20, &numbers[5]);
  mapmap.insert(30, &numbers[6]);
  mapmap.insert(40, &numbers[7]);

  REQUIRE(mapmap.size() == 8);

  // Front has smallest value
  REQUIRE(*(mapmap.front().second) == 1);

  // Back has largest value
  REQUIRE(*(mapmap.back().second) == 4);

  mapmap.erase(mapmap.begin());
  mapmap.erase(mapmap.begin());

  REQUIRE(mapmap.size() == 6);
  REQUIRE(*(mapmap.front().second) == 2);

  mapmap.clear();

  REQUIRE(mapmap.size() == 0);
}
