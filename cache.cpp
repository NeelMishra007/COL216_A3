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

int parseHexAddress(const string& address) {
    string hexPart = address;
    
    // Remove "0x" prefix if present
    if (hexPart.length() >= 2 && (hexPart.substr(0, 2) == "0x" || hexPart.substr(0, 2) == "0X")) {
        hexPart = hexPart.substr(2);
    }
    
    // Convert hex string to integer
    int addr_val = stoul(hexPart, nullptr, 16);
    
    // Format with leading zeros using stringstream (for debug output)
    stringstream ss;
    ss << "0x" << setfill('0') << setw(8) << hex << addr_val;
    string paddedAddress = ss.str();
    
    // Display the padded address (consider making this optional or removing for production)
    cout << "Padded Address: " << paddedAddress << endl;
    
    return addr_val;
}
void run(pair<char, const char*> entry, int core) {
    // Extract access type and address from the trace entry.
    char accessType = entry.first;
    string address = entry.second;
    
    // Parse the hexadecimal address.
    int addr = parseHexAddress(address);
    
    // Extract index and tag fields from the address.
    int index = (addr >> b) & ((1 << s) - 1); // index bits
    int tag = addr >> (s + b);                  // tag bits
    
    bool hit = false;
    int hit_line = -1;
    
    // Use a reference to the core's cache for easier access.
    Cache &cache = caches[core];
    
    if (accessType == 'R') {
        // Check every line in the set for a tag match.
        for (int i = 0; i < E; i++) {
            if (cache.valid[index][i] && cache.tags[index][i] == tag) {
                hit = true;
                hit_line = i;
                break;
            }
        }
        
        if (hit) {
            // Cache hit: Update the LRU ordering for the set.
            auto it = find(cache.lru[index].begin(), cache.lru[index].end(), hit_line);
            if (it != cache.lru[index].end()) {
                cache.lru[index].erase(it);
            }
            cache.lru[index].push_back(hit_line);
        }
        else {
            // Cache miss: Send a BusRd request to the bus.
            busQueue.push_back(BusReq{core, addr, BusReqType::BusRd});
            
            // Read miss handling code is commented out below.
            /* 
            int target_line = -1;
            // First try to use an invalid cache line.
            for (int i = 0; i < E; i++) {
                if (!cache.valid[index][i]) {
                    target_line = i;
                    break;
                }
            }
            // If all lines are valid, choose the LRU block.
            if (target_line == -1) {
                target_line = cache.lru[index].front();
                cache.lru[index].erase(cache.lru[index].begin());
                if (cache.dirty[index][target_line]) {
                    // Write-back logic here (e.g., write block back to memory).
                }
            }
            else {
                // Remove the chosen block from LRU.
                auto it = find(cache.lru[index].begin(), cache.lru[index].end(), target_line);
                if (it != cache.lru[index].end()) {
                    cache.lru[index].erase(it);
                }
            }
    
            // Load the block from memory and update metadata.
            cache.tags[index][target_line] = tag;
            cache.valid[index][target_line] = true;
            cache.dirty[index][target_line] = false; // Read miss: block is not dirty.
            // Mark the block as most recently used.
            cache.lru[index].push_back(target_line);
            */
        }        
    }
    else { // Write access
        // Search for a matching block in the set.
        for (int i = 0; i < E; i++) {
            if (cache.valid[index][i] && cache.tags[index][i] == tag) {
                hit = true;
                hit_line = i;
                break;
            }
        }
        
        if (hit) {
            // Cache hit: update the LRU order and mark the block as dirty.
            if (mesiState[core][index][hit_line] == MESIState::E || mesiState[core][index][hit_line] == MESIState::M) {
                auto it = find(cache.lru[index].begin(), cache.lru[index].end(), hit_line);
                if (it != cache.lru[index].end()) {
                    cache.lru[index].erase(it);
                }
                cache.lru[index].push_back(hit_line);
                cache.dirty[index][hit_line] = true;
            }
            else {
                // If the block is in the S state, send a BusUpgr request to upgrade it to M state.
                busQueue.push_back(BusReq{core, addr, BusReqType::BusUpgr});
            }

        }
        else {
            busQueue.push_back(BusReq{core, addr, BusReqType::BusRdX});
            // Write miss handling: find an invalid block first.
            /*int target_line = -1;
            for (int i = 0; i < E; i++) {
                if (!cache.valid[index][i]) {
                    target_line = i;
                    break;
                }
            }
            // If no invalid block exists, evict the least recently used block.
            if (target_line == -1) {
                target_line = cache.lru[index].front();
                cache.lru[index].erase(cache.lru[index].begin());
                if (cache.dirty[index][target_line]) {
                    // Write-back logic here (e.g., write block back to memory).
                }
            }
            else {
                // Remove the selected block from its current LRU position.
                auto it = find(cache.lru[index].begin(), cache.lru[index].end(), target_line);
                if (it != cache.lru[index].end()) {
                    cache.lru[index].erase(it);
                }
            }
    
            // Load the block from memory and update the cache metadata.
            cache.tags[index][target_line] = tag;
            cache.valid[index][target_line] = true;
            cache.dirty[index][target_line] = true; // Write miss: block becomes dirty.
            // Mark the newly loaded block as most recently used.
            cache.lru[index].push_back(target_line);*/
        }
    }
}
