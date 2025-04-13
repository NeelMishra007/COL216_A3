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
#include "cache.hpp"

void bus() {
    for (int i = 0; i < busDataQueue.size(); i++) {
        BusData &busData = busDataQueue[i];
        
        // Check if the stall counter has reached zero
        if (busData.stalls == 0) {
            int core = busData.coreId;
            int addr = busData.address;
            bool isWrite = busData.write;
            bool isWriteback = busData.writeback;
            
            // Process completed bus data transfer
            if (!isWriteback) {
                // Extract index and tag fields from the address
                int index = (addr >> b) & ((1 << s) - 1);
                int tag = addr >> (s + b);
                
                // Process read or write miss
                if (isWrite) {
                    handle_write_miss(core, index, tag);
                    mesiState[core][index][0] = MESIState::M; // Set to Modified state
                } else {
                    handle_read_miss(core, index, tag);
                    
                    // Check if other caches have the data to determine state
                    bool otherCachesHaveData = false;
                    for (int j = 0; j < 4; j++) {
                        if (j != core) {
                            for (int k = 0; k < E; k++) {
                                if (caches[j].tags[index][k] == tag && mesiState[j][index][k] != MESIState::I) {
                                    otherCachesHaveData = true;
                                    break;
                                }
                            }
                        }
                        if (otherCachesHaveData) break;
                    }
                    
                    // Set to Exclusive if no other cache has it, Shared otherwise
                    if (otherCachesHaveData) {
                        mesiState[core][index][0] = MESIState::S;
                    } else {
                        mesiState[core][index][0] = MESIState::E;
                    }
                }
                
                // Reset the stall flag for the core
                caches[core].stall = false;
                clockCycles[core]++; // Increment clock cycle for the core
                
                // Remove current write marker if this was a write operation
                if (isWrite) {
                    curr_write.erase(addr);
                }
            } else {
                // Handle writeback completion
                // Just clear stall flag as writeback is complete
                caches[core].stall = false;
            }
            
            // Remove this entry from the queue
            busDataQueue.erase(busDataQueue.begin() + i);
            i--; // Adjust index since we removed an element
        } else {
            // Decrement stall counter
            busDataQueue[i].stalls--;
        }
    }

    while (busQueue.size()) {
        BusReq busReq = busQueue.front();
        busQueue.erase(busQueue.begin());
    
        int core = busReq.coreId;
        int addr = busReq.address;
        BusReqType type = busReq.type;
    
        int index = (addr >> b) & ((1 << s) - 1);
        int tag = addr >> (s + b);                  
        
        if (curr_write.find(addr) != curr_write.end()) {
            caches[core].stall = true; // Set the stall flag for the core.
            continue;
        }
        
        if (type == BusReqType::BusRd) {
            bool found = false;
            for (int i = 0; i < 4; i++) {
                if (i != core) {
                    for (int j = 0; j < E; j++) {
                        if (caches[i].tags[index][j] == tag && mesiState[i][index][j] != MESIState::I) {
                            found = true;
    
                            if (mesiState[i][index][j] == MESIState::M) {
                                // Send BusRd to share the line with the requesting core.
                                mesiState[i][index][j] = MESIState::S; // Fixed: Using = instead of ==
                                caches[i].stall = true; // Set the stall flag for the core.
                                busDataQueue.push_back(BusData{addr, i, false, true, 100}); // Writeback data.
                            } 
                            else if (mesiState[i][index][j] == MESIState::E) {
                                // Send BusRd to share the line with the requesting core.
                                mesiState[i][index][j] = MESIState::S; // Fixed: Using = instead of ==
                            }
                            
                            caches[core].stall = true; // Set the stall flag for the requesting core.
                            busDataQueue.push_back(BusData{addr, core, false, false, 1 << (b - 1)});  // Send data to the requesting core.
                            break;
                        }
                    }
                }
                if (found) {
                    break;
                }
            }
            if (!found) {
                caches[core].stall = true;
                busDataQueue.push_back(BusData{addr, core, false, false, 100}); // Send data to the requesting core.
            }
        } 
        else if (type == BusReqType::BusUpgr) {
            // Find the cache line that needs to be upgraded
            int target_line = -1;
            for (int j = 0; j < E; j++) {
                if (caches[core].tags[index][j] == tag && mesiState[core][index][j] == MESIState::S) {
                    target_line = j;
                    break;
                }
            }
    
            if (target_line != -1) {
                // Invalidate copies in other caches
                for (int i = 0; i < 4; i++) {
                    if (i != core) {
                        for (int j = 0; j < E; j++) {
                            if (caches[i].tags[index][j] == tag && mesiState[i][index][j] != MESIState::I) {
                                mesiState[i][index][j] = MESIState::I; // Invalidate the line in other caches.
                            }
                        }
                    }
                }
                
                // Upgrade the state to Modified
                mesiState[core][index][target_line] = MESIState::M; // Fixed: Using the correct line index
                caches[core].dirty[index][target_line] = true; // Mark the line as dirty.
            }
        }
        else if (type == BusReqType::BusRdX) {
            bool found = false;
            curr_write.insert(addr); // Mark the address as currently being written to.
            
            // Check if any other cache has this line and invalidate it
            for (int i = 0; i < 4; i++) {
                if (i != core) {
                    for (int j = 0; j < E; j++) {
                        if (caches[i].tags[index][j] == tag && mesiState[i][index][j] != MESIState::I) {
                            found = true;
                            
                            // If the line is in M state, need to writeback
                            if (mesiState[i][index][j] == MESIState::M) {
                                caches[i].stall = true;
                                busDataQueue.push_back(BusData{addr, i, false, true, 100}); // Writeback data
                            }
                            
                            // Invalidate the line
                            mesiState[i][index][j] = MESIState::I;
                        }
                    }
                }
            }
            
            caches[core].stall = true; // Set the stall flag for the requesting core.
            
            if (found) {
                // Send data from another cache to the requesting core
                busDataQueue.push_back(BusData{addr, core, true, false, 1 << (b - 1)});
            } else {
                // Get data from memory
                busDataQueue.push_back(BusData{addr, core, true, false, 100});
            }
        }
    }
}