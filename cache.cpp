#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include "main.hpp"
#include "bus.hpp"

using namespace std;

// Declaring the external variable defined in bus.cpp
extern vector<int> corePendingOperation;
extern vector<int> num_reads;
extern vector<int> num_writes;
extern vector<int> cache_misses;
extern vector<int> cache_evictions;
extern vector<int> writebacks;
extern vector<long long> data_traffic_bytes;
int cycle2 = 0;
int parseHexAddress(const string &address)
{
    string hexPart = address;

    // Remove "0x" prefix if present
    if (hexPart.length() >= 2 && (hexPart.substr(0, 2) == "0x" || hexPart.substr(0, 2) == "0X"))
    {
        hexPart = hexPart.substr(2);
    }

    // Convert hex string to integer
    int addr_val = stoul(hexPart, nullptr, 16);

    // Format with leading zeros using stringstream (for debug output)
    stringstream ss;
    ss << "0x" << setfill('0') << setw(8) << hex << addr_val;
    string paddedAddress = ss.str();

    // Display the padded address (consider making this optional or removing for production)
    // cout << "Padded Address: " << paddedAddress << endl;

    return addr_val;
}

int handle_read_miss(int core, int index, int tag)
{
    Cache &cache = caches[core];
    int target_line = -1;

    // Try to find an invalid line first
    for (int i = 0; i < E; i++)
    {
        if (mesiState[core][index][i] == MESIState::I)
        {
            target_line = i;
            break;
        }
    }

    // If all lines are valid, evict the least recently used (LRU) block
    if (target_line == -1)
    {
        target_line = cache.lru[index].front();
        cache.lru[index].erase(cache.lru[index].begin());
        cache_evictions[core]++; // Increment eviction counter

        if (cache.dirty[index][target_line])
        {
            caches[core].stall = true; // Set the stall flag for the requesting core
            int old_tag = cache.tags[index][target_line];
            int old_addr = (old_tag << (s + b)) | (index << b);
            busDataQueue.push_back(BusData{old_addr, core, false, true, 100}); // Writeback data
            writebacks[core]++;                                                // Increment writeback counter
            data_traffic_bytes[core] += cache.blockSize;                       // Add writeback traffic
        }
    }
    else
    {
        // Remove the chosen block from LRU if it exists there
        auto it = find(cache.lru[index].begin(), cache.lru[index].end(), target_line);
        if (it != cache.lru[index].end())
        {
            cache.lru[index].erase(it);
        }
    }

    // Load the block from memory and update metadata
    cache.tags[index][target_line] = tag;
    //cout << " " << "hi" << cache.tags[index][target_line] << endl;
    cache.dirty[index][target_line] = false; // It's a read miss
    cache.lru[index].push_back(target_line); // Mark as most recently used
    return target_line; // Return the target line index
}

int handle_write_miss(int core, int index, int tag)
{
    Cache &cache = caches[core];
    int target_line = -1;

    // Try to find an invalid line first
    for (int i = 0; i < E; i++)
    {
        if (mesiState[core][index][i] == MESIState::I)
        {
            target_line = i;
            break;
        }
    }

    // If no invalid line, evict the least recently used (LRU) block
    if (target_line == -1)
    {
        target_line = cache.lru[index].front();
        cache.lru[index].erase(cache.lru[index].begin());
        cache_evictions[core]++; // Increment eviction counter
        if (cache.dirty[index][target_line])
        {
            int old_tag = cache.tags[index][target_line];
            int old_addr = (old_tag << (s + b)) | (index << b);
            busDataQueue.push_back(BusData{old_addr, core, false, true, 100}); // Writeback data
        }
    }
    else
    {
        // Remove the selected block from its LRU position if it exists
        auto it = find(cache.lru[index].begin(), cache.lru[index].end(), target_line);
        if (it != cache.lru[index].end())
        {
            cache.lru[index].erase(it);
        }
    }

    // Update the cache metadata for the new block
    cache.tags[index][target_line] = tag;
    cache.dirty[index][target_line] = true;  // It's a write miss
    cache.lru[index].push_back(target_line); // Mark as most recently used
    return target_line;
}

void run(pair<char, const char *> entry, int core)
{
    cycle2++;
    // Extract access type and address from the trace entry
    char accessType = entry.first;
    string address = entry.second;

    // Parse the hexadecimal address
    int addr = parseHexAddress(address);
    // If this core already has a pending operation, skip issuing a new request
    if (corePendingOperation[core] != -1)
    {
        return;
    }
    // Increment read/write counters
    if (cycle2 % 100000 == 0)
    {
        //cout << "Core " << core << " Access Type: " << accessType << ", Address: " << address << " " << caches[core].stall << endl;
    }

    // Extract index and tag fields from the address
    int index = (addr >> b) & ((1 << s) - 1); // index bits
    int tag = addr >> (s + b);                // tag bits
    //cout << index << " " << tag << endl;
    bool hit = false;
    int hit_line = -1;

    // Use a reference to the core's cache for easier access
    Cache &cache = caches[core];

    if (accessType == 'R')
    {
        // Check every line in the set for a tag match
        for (int i = 0; i < E; i++)
        {
            if (mesiState[core][index][i] != MESIState::I && cache.tags[index][i] == tag)
            {
                hit = true;
                hit_line = i;
                break;
            }
        }

        if (hit)
        {
            // Cache hit: Update the LRU ordering for the set
            auto it = find(cache.lru[index].begin(), cache.lru[index].end(), hit_line);
            if (it != cache.lru[index].end())
            {
                cache.lru[index].erase(it);
            }
            cache.lru[index].push_back(hit_line);
            clockCycles[core]++;
            //caches[core].stall = false;
        }
        else
        {
            //cout << "Core " << core << " Access Type: " << accessType << ", Address: " << address << " " << caches[core].stall << endl;
            busQueue.push_back(BusReq{core, addr, BusReqType::BusRd});
            caches[core].stall = true; // Set the stall flag for the requesting core
        }
    }
    else
    { // Write access
        // Search for a matching block in the set
        for (int i = 0; i < E; i++)
        {
            if (mesiState[core][index][i] != MESIState::I && cache.tags[index][i] == tag)
            {
                hit = true;
                hit_line = i;
                break;
            }
        }

        if (hit)
        {
            if (cycle2 % 100000 == 0)
            {
                //cout << "Core " << core << " Access Type: " << accessType << ", Address: " << address << " " << "hit" << endl;
            }

            // Cache hit: update the LRU order and mark the block as dirty
            if (mesiState[core][index][hit_line] == MESIState::E || mesiState[core][index][hit_line] == MESIState::M)
            {
                auto it = find(cache.lru[index].begin(), cache.lru[index].end(), hit_line);
                if (it != cache.lru[index].end())
                {
                    cache.lru[index].erase(it);
                }
                cache.lru[index].push_back(hit_line);
                cache.dirty[index][hit_line] = true;
                if (mesiState[core][index][hit_line] == MESIState::E)
                {
                    mesiState[core][index][hit_line] = MESIState::M; // Upgrade to M state
                }
                clockCycles[core]++;
                //caches[core].stall = false;
            }
            else
            {
                // If the block is in the S state, send a BusUpgr request to upgrade it to M state
                busQueue.push_back(BusReq{core, addr, BusReqType::BusUpgr});
            }
        }
        else
        {
            //cout << "Core " << core << " Access Type: " << accessType << ", Address: " << address << " " << caches[core].stall << endl;
            busQueue.push_back(BusReq{core, addr, BusReqType::BusRdX});
            caches[core].stall = true;
        }
    }
}