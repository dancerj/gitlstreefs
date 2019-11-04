CFLAGS=$(pkg-config fuse --libs --cflags) -D_FILE_OFFSET_BITS=64 -std=c++17 -Wall -Werror
all:
	ninja

clean:
	ninja -t clean

check-syntax:
	$(CXX) -c $(CFLAGS) $(CHK_SOURCES) -o /dev/null 

.PHONY: all clean
