#include <iostream>

#include "util/sorted_map.hh"

template <class Key, class T,
          std::enable_if_t<std::is_pointer_v<T>> * = nullptr>
void print_list(SimpleSSD::map_list<Key, T> &queue) {
  for (auto iter : queue) {
    std::cout << "(" << iter.first << ": " << *(uint32_t *)iter.second << ") ";
  }

  std::cout << std::endl;
}

template <class Key, class T,
          std::enable_if_t<std::is_pointer_v<T>> * = nullptr>
void print_map(SimpleSSD::map_map<Key, T> &queue) {
  for (auto iter : queue) {
    std::cout << "(" << iter.first << ": " << *(uint32_t *)iter.second << ") ";
  }

  std::cout << std::endl;
}

int main(int, char *[]) {
  SimpleSSD::map_list<uint32_t, uint32_t *> list;
  SimpleSSD::map_map<uint32_t, uint32_t *> map(
      [](const uint32_t *a, const uint32_t *b) -> bool { return *a < *b; });
  std::array<uint32_t, 8> numbers = {4, 3, 2, 1, 4, 3, 2, 1};

  // TEST 1 - push_back
  list.push_back(1, &numbers[0]);
  list.push_back(2, &numbers[1]);
  list.push_back(3, &numbers[2]);
  list.push_back(4, &numbers[3]);

  print_list(list);

  // TEST 2 - push_front
  list.push_front(10, &numbers[4]);
  list.push_front(20, &numbers[5]);
  list.push_front(30, &numbers[6]);
  list.push_front(40, &numbers[7]);

  print_list(list);

  // TEST 3 - pop_back
  list.pop_back();

  print_list(list);

  // TEST 4 - pop_front
  list.pop_front();

  print_list(list);

  // TEST 5 - erase
  list.erase(list.begin());

  print_list(list);

  // TEST 6 - clear
  list.clear();

  print_list(list);

  // TEST 1 - push_back
  map.insert(1, &numbers[0]);
  map.insert(2, &numbers[1]);
  map.insert(3, &numbers[2]);
  map.insert(4, &numbers[3]);
  map.insert(10, &numbers[4]);
  map.insert(20, &numbers[5]);
  map.insert(30, &numbers[6]);
  map.insert(40, &numbers[7]);

  print_map(map);

  // TEST 2 - erase
  map.erase(map.begin());

  print_map(map);

  // TEST 3 - clear
  map.clear();

  print_map(map);

  return 0;
}
