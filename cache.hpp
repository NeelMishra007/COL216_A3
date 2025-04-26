#ifndef CACHE_HPP
#define CACHE_HPP

#include <vector>
#include <utility>


void run(std::pair<char, const char*> entry, int core);

int handle_read_miss(int core, int index, int tag);

// Handles a write miss for a given core, index, and tag
int handle_write_miss(int core, int index, int tag);

// Runs a single memory operation (read/write) for a given core
#endif // CACHE_HPP