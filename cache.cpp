#include <iostream>      
#include <string>        
#include <vector>      
#include <sstream>      
#include <iomanip>       
#include <algorithm>     
#include <cstring>       
#include <cstdlib>     
#include "main.hpp"   

using namespace std;

unsigned int parseHexAddress(const string& address) {
    string hexPart = address;
    
    // Remove "0x" prefix if present
    if (hexPart.length() >= 2 && (hexPart.substr(0, 2) == "0x" || hexPart.substr(0, 2) == "0X")) {
        hexPart = hexPart.substr(2);
    }
    
    // Convert hex string to integer
    unsigned int addr_val = stoul(hexPart, nullptr, 16);
    
    // Format with leading zeros using stringstream (for debug output)
    stringstream ss;
    ss << "0x" << setfill('0') << setw(8) << hex << addr_val;
    string paddedAddress = ss.str();
    
    // Display the padded address (consider making this optional or removing for production)
    cout << "Padded Address: " << paddedAddress << endl;
    
    return addr_val;
}
void runSimulation(vector<vector<unsigned int>> &tags, vector<vector<bool>> &valid, 
    vector<vector<int>> &lru, vector<vector<bool>> &dirty, pair<char, const char*> entry) {

    int sets = 1 << s;                        
    int blockSize = 1 << b;                     // Block size = 2^b bytes    
    
    for (int i = 0; i < sets; i++) {
        for (int j = 0; j < E; j++) {
            lru[i].push_back(j);
        }
    }
    
    char accessType = entry.first;
    string address = entry.second;

    unsigned int addr = parseHexAddress(address);

    unsigned int index = (addr >> b) & ((1 << s) - 1); // Extract index bits
    unsigned int tag = addr >> (s + b);                // Extract tag bits
    bool hit = false;
    int hit_line = -1;

    if (accessType == 'R') {
        for (int i = 0; i < E; i++) {
            if (valid[index][i] && tags[index][i] == tag) {
                hit = true;
                hit_line = i;
                break;
            }
        }
        if (hit) {
            auto it = find(lru[index].begin(), lru[index].end(), hit_line);
            if (it != lru[index].end()) {
                lru[index].erase(it);
            }
            lru[index].push_back(hit_line); // Update LRU
        }
        else {
            // Read miss: try to find an invalid block first.
            int target_line = -1;
            for (int i = 0; i < E; i++) {
                if (!valid[index][i]) {
                    target_line = i;
                    break;
                }
            }
            // If no invalid block is available, choose the LRU block (front of vector)
            if (target_line == -1) {
                target_line = lru[index].front();
                lru[index].erase(lru[index].begin());
                // If the block is dirty, write it back before replacement.
                if (dirty[index][target_line]) {
                    // Write-back logic here (e.g., write block back to memory)
                }
            }
            else {
                // Remove the chosen block from its current position in LRU.
                auto it = find(lru[index].begin(), lru[index].end(), target_line);
                if (it != lru[index].end()) {
                    lru[index].erase(it);
                }
            }
    
            // Load the block from memory and update cache metadata.
            tags[index][target_line] = tag;
            valid[index][target_line] = true;
            dirty[index][target_line] = false; // Read miss: block is not dirty
            // Update LRU: mark the newly loaded block as most recently used.
            lru[index].push_back(target_line);
        }        
    }
    else {
        for (int i = 0; i < E; i++) {
            if (valid[index][i] && tags[index][i] == tag) {
                hit = true;
                hit_line = i;
                break;
            }
        }
        if (hit) {
            auto it = find(lru[index].begin(), lru[index].end(), hit_line);
            if (it != lru[index].end()) {
                lru[index].erase(it);
            }
            lru[index].push_back(hit_line); // Update LRU
            dirty[index][hit_line] = true; // Mark the block as dirty
        }
        else {
            // Write miss: try to find an invalid block first.
            int target_line = -1;
            for (int i = 0; i < E; i++) {
                if (!valid[index][i]) {
                    target_line = i;
                    break;
                }
            }
            // If no invalid block is available, choose the LRU block (front of vector)
            if (target_line == -1) {
                target_line = lru[index].front();
                lru[index].erase(lru[index].begin());
                // If the block is dirty, write it back before replacement.
                if (dirty[index][target_line]) {
                    // Write-back logic here (e.g., write block back to memory)
                }
            }
            else {
                // Remove the chosen block from its current position in LRU.
                auto it = find(lru[index].begin(), lru[index].end(), target_line);
                if (it != lru[index].end()) {
                    lru[index].erase(it);
                }
            }
    
            // Load the block from memory and update cache metadata.
            tags[index][target_line] = tag;
            valid[index][target_line] = true;
            dirty[index][target_line] = true; // Write miss: block is dirty
            // Update LRU: mark the newly loaded block as most recently used.
            lru[index].push_back(target_line);
        }

    }
}
