#ifndef MAIN_HPP
#define MAIN_HPP

#include <vector>
#include <utility>

// Bring in the standard namespace types you need.
using namespace std;
// External configuration parameters. These should be defined in main.cpp.
extern int s;  // Number of index bits: number of sets = 2^s
extern int b;  // Number of block offset bits: block size = 2^b bytes
extern int E;  // Associativity (number of lines per set)

// External trace inputs for four cores.
extern vector<pair<char, const char*>> trace1;
extern vector<pair<char, const char*>> trace2;
extern vector<pair<char, const char*>> trace3;
extern vector<pair<char, const char*>> trace4;

// Structure to hold a cache's per-core data.
struct Cache {
    int sets;                           // Number of sets = 2^s
    int blockSize;                      // Block size = 2^b bytes
    bool stall;                       
    vector<vector<unsigned int>> tags;  // Tag storage [set][line]
    vector<vector<bool>> valid;         // Valid bits [set][line]
    vector<vector<int>> lru;            // LRU ordering [set] holds line indices
    vector<vector<bool>> dirty;         // Dirty bits [set][line]

    // Initialize the cache based on the global parameters s, b, and E.
    void init() {
        sets = 1 << s;            // Number of sets = 2^s
        blockSize = 1 << b;       // Block size = 2^b bytes

        // Resize and initialize cache data structures.
        tags.assign(sets, vector<unsigned int>(E, 0));
        valid.assign(sets, vector<bool>(E, false));
        dirty.assign(sets, vector<bool>(E, false));

        // Initialize the LRU ordering: for each set, create an ordering of block indices 0 to E-1.
        lru.clear();
        lru.resize(sets);
        for (int i = 0; i < sets; i++) {
            lru[i].clear();
            for (int j = 0; j < E; j++) {
                lru[i].push_back(j);
            }
        }
    }
};

extern Cache caches[4];

enum class MESIState {
    M,E,S,I
};
extern vector<vector<MESIState>> mesiState[4]; 

extern vector<int> instructions;
extern vector<int> clockCycles;

extern vector<int> num_reads;
extern vector<int> num_writes;
extern vector<int> cache_misses;
extern vector<int> cache_evictions;
extern vector<int> writebacks;
extern vector<int> bus_invalidations;
extern vector<long long> data_traffic_bytes;
extern int total_bus_transactions;
extern long long total_bus_traffic_bytes;

#endif // MAIN_HPP