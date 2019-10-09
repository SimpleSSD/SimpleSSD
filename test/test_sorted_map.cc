#include <iostream>

#include "util/sorted_map.hh"

template <class Key, class T,
          std::enable_if_t<std::is_pointer_v<T>> * = nullptr>
void print_queue(SimpleSSD::map_list<Key, T> &queue) {
  for (auto iter : queue) {
    std::cout << "(" << iter.first << ": " << *(uint32_t *)iter.second << ") ";
  }

  std::cout << std::endl;
}

int main(int, char *[]) {
  SimpleSSD::map_list<uint32_t, uint32_t *> queue;
  // SimpleSSD::map_list queue;
  std::array<uint32_t, 8> numbers = {4, 3, 2, 1, 4, 3, 2, 1};

  // TEST 1 - push_back
  queue.push_back(1, &numbers[0]);
  queue.push_back(2, &numbers[1]);
  queue.push_back(3, &numbers[2]);
  queue.push_back(4, &numbers[3]);

  print_queue(queue);

  // TEST 2 - push_front
  queue.push_front(10, &numbers[4]);
  queue.push_front(20, &numbers[5]);
  queue.push_front(30, &numbers[6]);
  queue.push_front(40, &numbers[7]);

  print_queue(queue);

  // TEST 3 - pop_back
  queue.pop_back();

  print_queue(queue);

  // TEST 4 - pop_front
  queue.pop_front();

  print_queue(queue);

  // TEST 5 - erase
  queue.erase(queue.begin());

  print_queue(queue);

  // TEST 7 - clear
  queue.clear();

  print_queue(queue);

  return 0;
}
