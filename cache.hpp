#ifndef CACHE_HPP
#define CACHE_HPP

#include <utility>

void handle_read_miss(int core, int index, int tag);

// Handles a write miss for a given core, index, and tag
void handle_write_miss(int core, int index, int tag);

// Runs a single memory operation (read/write) for a given core
void run(std::pair<char, const char*> entry, int core);

#endif // CACHE_HPP