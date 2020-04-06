// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Copyright (C) 2019 CAMELab
 *
 * Author: Donghyun Gouk <kukdh1@camelab.org>
 */

#include "util/sorted_map.hh"

#include <catch2/catch.hpp>

TEST_CASE("SortedMap") {
  using namespace SimpleSSD;

  map_list<uint32_t, uint32_t *> maplist;
  map_map<uint32_t, uint32_t *> mapmap(
      [](const uint32_t *a, const uint32_t *b) -> bool { return *a < *b; });

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
}
