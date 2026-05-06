#include <cassert>
#include <iostream>

#include "skynet/StaleObject.hpp"

int main() {
  std::cout << "[stale_object_mutate] start\n";
  skynet::StaleObject<int> obj;

  std::cout << "[stale_object_mutate] set/get baseline\n";
  obj.set(10);
  assert(obj.get() == 10);
  std::cout << "  lane0 = " << obj.get() << "\n";

  std::cout << "[stale_object_mutate] expand to 4 lanes\n";
  skynet::clause_stale_object_set(4, &obj);
  assert(obj.__get(0) == 10);
  assert(obj.__get(1) == 10);
  assert(obj.__get(2) == 10);
  assert(obj.__get(3) == 10);
  std::cout << "  lanes after expansion: " << obj.__get(0) << ", "
            << obj.__get(1) << ", " << obj.__get(2) << ", " << obj.__get(3)
            << "\n";

  std::cout << "[stale_object_mutate] mutate lane 2 with __mutate (+5)\n";
  obj.__set(2, 20);
  obj.__mutate(2, [](int &x) { x += 5; });
  assert(obj.__get(2) == 25);
  std::cout << "  lane2 = " << obj.__get(2) << "\n";

  std::cout << "[stale_object_mutate] mutate active lane with mutate (*3)\n";
  obj.mutate([](int &x) { x *= 3; });
  assert(obj.__get(0) == 30);
  assert(obj.__get(1) == 10);
  assert(obj.__get(2) == 25);
  assert(obj.__get(3) == 10);
  std::cout << "  lanes after mutate: " << obj.__get(0) << ", " << obj.__get(1)
            << ", " << obj.__get(2) << ", " << obj.__get(3) << "\n";

  std::cout << "[stale_object_mutate] all checks passed\n";

  return 0;
}
