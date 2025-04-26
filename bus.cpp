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
// Track pending bus operations per core
vector<int> corePendingOperation(4, -1); // -1 indicates no pending operation
bool bus_busy = false;
void bus()
{
    while (busQueue.size())
    {
        BusReq busReq = busQueue.front();
        busQueue.erase(busQueue.begin());

        //cout << "busReq: " << busReq.coreId << " " << busReq.address << " " << (int)busReq.type << endl;
        //cout << bus_busy << endl;
        int core = busReq.coreId;
        int addr = busReq.address;
        BusReqType type = busReq.type;

        int index = (addr >> b) & ((1 << s) - 1);
        int tag = addr >> (s + b);

        if (type == BusReqType::BusUpgr)
        {
            // Find the cache line that needs to be upgraded
            int target_line = -1;
            for (int j = 0; j < E; j++)
            {
                if (caches[core].tags[index][j] == tag && mesiState[core][index][j] == MESIState::S)
                {
                    target_line = j;
                    break;
                }
            }

            if (target_line != -1)
            {
                // Invalidate copies in other caches
                for (int i = 0; i < 4; i++)
                {
                    if (i != core)
                    {
                        for (int j = 0; j < E; j++)
                        {
                            if (caches[i].tags[index][j] == tag && mesiState[i][index][j] != MESIState::I)
                            {
                                mesiState[i][index][j] = MESIState::I; // Invalidate the line in other caches
                                bus_invalidations[i]++;                // Increment invalidation counter
                            }
                        }
                    }
                }

                // Upgrade the state to Modified
                mesiState[core][index][target_line] = MESIState::M;
                caches[core].dirty[index][target_line] = true; // Mark the line as dirty
                caches[core].stall = false;                    // Clear the stall since operation completes immediately
                corePendingOperation[core] = -1;               // Clear pending operation for this core
            }
            total_bus_transactions++; // Increment bus transaction counter
        }

        if (bus_busy)
        {
            caches[core].stall = true;
            continue;
        }
        corePendingOperation[core] = addr;
        if (type == BusReqType::BusRd)
        {
            bus_busy = true;
            total_bus_transactions++; // Increment bus transaction counter
            cache_misses[core]++;     // Increment miss counter
            // cout << core << "hi2" << endl;
            bool found = false;
            for (int i = 0; i < 4; i++)
            {
                if (i != core)
                {
                    for (int j = 0; j < E; j++)
                    {
                        if (caches[i].tags[index][j] == tag && mesiState[i][index][j] != MESIState::I)
                        {
                            found = true;
                            caches[core].stall = true; // Set the stall flag for the requesting core
                            // cout << "yes" << endl;
                            busDataQueue.push_back(BusData{addr, core, false, false, 1 << (b - 1)}); // Send data to the requesting core

                            if (mesiState[i][index][j] == MESIState::M)
                            {
                                // Send BusRd to share the line with the requesting core
                                mesiState[i][index][j] = MESIState::S;
                                caches[i].stall = true;                                     // Set the stall flag for the core
                                busDataQueue.push_back(BusData{addr, i, false, true, 100}); // Writeback data
                                corePendingOperation[i] = addr;
                            }
                            else if (mesiState[i][index][j] == MESIState::E)
                            {
                                // Send BusRd to share the line with the requesting core
                                mesiState[i][index][j] = MESIState::S;
                            }

                            break;
                        }
                    }
                }
                if (found)
                {
                    break;
                }
            }
            if (!found)
            {
                caches[core].stall = true;
                busDataQueue.push_back(BusData{addr, core, false, false, 100}); // Send data to the requesting core
                total_bus_traffic_bytes += caches[busReq.coreId].blockSize;     // Add request traffic
            }
        }
        else if (type == BusReqType::BusRdX)
        {
            bus_busy = true;
            total_bus_transactions++; // Increment bus transaction counter
            bool found = false;
            bool foundm = false;
            cache_misses[core]++; // Increment miss counter
            // Check if any other cache has this line and invalidate it
            for (int i = 0; i < 4; i++)
            {
                if (i != core)
                {
                    for (int j = 0; j < E; j++)
                    {
                        if (caches[i].tags[index][j] == tag && mesiState[i][index][j] != MESIState::I)
                        {
                            // Invalidate the line
                            if (mesiState[i][index][j] == MESIState::M)
                            {
                                foundm = true;
                                // Send BusRd to share the line with the requesting core
                                caches[i].stall = true;                                     // Set the stall flag for the core
                                busDataQueue.push_back(BusData{addr, i, false, true, 100}); // Writeback data
                                total_bus_traffic_bytes += caches[i].blockSize;             // Add request traffic
                                corePendingOperation[i] = addr;
                            }

                            mesiState[i][index][j] = MESIState::I;
                            bus_invalidations[i]++; // Increment invalidation counter
                        }
                    }
                }
            }
            caches[core].stall = true; // Set the stall flag for the requesting core

            busDataQueue.push_back(BusData{addr, core, true, false, 100});
            total_bus_traffic_bytes += caches[core].blockSize; // Add request traffic
        }
    }
    // Handle a single bus data operation if any
    if (!busDataQueue.empty())
    {
        BusData &busData = busDataQueue.front();
        //cout << "busData: " << busData.coreId << " " << busData.address << " " << (int)busData.writeback << endl;

        // Check if the stall counter has reached zero
        if (busData.stalls == 0)
        {
            int core = busData.coreId;
            int addr = busData.address;
            bool isWrite = busData.write;
            bool isWriteback = busData.writeback;

            // Process completed bus data transfer
            if (!isWriteback)
            {
                // Extract index and tag fields from the address
                int index = (addr >> b) & ((1 << s) - 1);
                int tag = addr >> (s + b);
                data_traffic_bytes[core] += caches[core].blockSize; // Add writeback traffic
                // Process read or write miss
                if (isWrite)
                {
                    handle_write_miss(core, index, tag);
                    mesiState[core][index][0] = MESIState::M; // Set to Modified state
                }
                else
                {
                    handle_read_miss(core, index, tag);

                    // Check if other caches have the data to determine state
                    bool otherCachesHaveData = false;
                    for (int j = 0; j < 4; j++)
                    {
                        if (j != core)
                        {
                            for (int k = 0; k < E; k++)
                            {
                                if (caches[j].tags[index][k] == tag && mesiState[j][index][k] != MESIState::I)
                                {
                                    otherCachesHaveData = true;
                                    break;
                                }
                            }
                        }
                        if (otherCachesHaveData)
                            break;
                    }

                    // Set to Exclusive if no other cache has it, Shared otherwise
                    if (otherCachesHaveData)
                    {
                        mesiState[core][index][0] = MESIState::S;
                    }
                    else
                    {
                        mesiState[core][index][0] = MESIState::E;
                    }
                }

                // Reset the stall flag for the core
                caches[core].stall = false;
                clockCycles[core]++; // Increment clock cycle for the core

                // Clear pending operation for this core
                corePendingOperation[core] = -1;
            }
            else
            {
                writebacks[core]++;                                 // Increment writeback counter
                data_traffic_bytes[core] += caches[core].blockSize; // Add writeback traffic
                // Handle writeback completion
                // Just clear stall flag as writeback is complete
                caches[core].stall = false;

                // Clear pending operation for this core
                corePendingOperation[core] = -1;
            }

            // Remove this entry from the queue
            busDataQueue.erase(busDataQueue.begin());
            if (busDataQueue.empty())
            {
                bus_busy = false;
            }
        }
        else
        {
            // Decrement stall counter
            busData.stalls--;
        }
    }
}
