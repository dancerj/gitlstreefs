#include "scoped_timer.h"

#include <iostream>

int main() {
  { scoped_timer::ScopedTimer timer("hello world"); }
  std::cout << scoped_timer::ScopedTimer::dump() << std::endl;
}
