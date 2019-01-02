#include "cached_file.h"

int main(int argc, char** argv) {
  Cache c(".cache/");
  if (c.Gc()) {
    exit(EXIT_SUCCESS);
  } else {
    exit (EXIT_FAILURE);
  }
}
