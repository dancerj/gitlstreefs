#include "scoped_timer.h"

#include <iostream>

int main() {
  {
    ScopedTimer timer("hello world");
  }
  std::cout << ScopedTimer::dump() << std::endl;
}
