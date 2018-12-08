#include "scoped_timer.h"

int main() {
  ScopedTimer timer("hello world");
  std::cout << timer.get() << std::endl;
}
