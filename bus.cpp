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
int cycle = 0;
void bus()
{
    cycle++;
    while (busQueue.size())
    {
        BusReq busReq = busQueue.front();
        busQueue.erase(busQueue.begin());

        // cout << "busReq: " << busReq.coreId << " " << busReq.address << " " << (int)busReq.type << endl;
        // cout << bus_busy << endl;
        int core = busReq.coreId;
        int addr = busReq.address;
        BusReqType type = busReq.type;

        // if (busReq.address == 0x7f13d768)
        //{
        // cout << "Core " << core << " Cycle: " << cycle << ", Instruction: " << instructions[core] << endl;
        //}

        int index = (addr >> b) & ((1 << s) - 1);
        int tag = addr >> (s + b);

        if (bus_busy)
        {
            caches[core].stall = true;
            // if (cycle % 100000 == 0)
            //{
            // cout << "Core " << core << " Cycle: " << cycle << ", Instruction: " << instructions[core] << endl;
            //}
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
                            data_traffic_bytes[i] += caches[i].blockSize;
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
            }
        }
        else if (type == BusReqType::BusRdX)
        {
            bus_busy = true;
            total_bus_transactions++; // Increment bus transaction counter
            bool found = false;
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
                            found = true;
                            // Invalidate the line
                            if (mesiState[i][index][j] == MESIState::M)
                            {
                                // Send BusRd to share the line with the requesting core
                                caches[i].stall = true;                                     // Set the stall flag for the core
                                busDataQueue.push_back(BusData{addr, i, false, true, 100}); // Writeback data
                                corePendingOperation[i] = addr;
                            }

                            mesiState[i][index][j] = MESIState::I;
                            // bus_invalidations[i]++; // Increment invalidation counter
                        }
                    }
                }
            }
            caches[core].stall = true; // Set the stall flag for the requesting core
            if (found)
                bus_invalidations[core]++; // Increment invalidation counter
            busDataQueue.push_back(BusData{addr, core, true, false, 100});
        }
        else if (type == BusReqType::BusUpgr)
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
                total_bus_transactions++; // Increment bus transaction counter

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
        }
    }
    // Handle a single bus data operation if any
    if (!busDataQueue.empty())
    {
        BusData &busData = busDataQueue.front();
        // cout << "busData: " << busData.coreId << " " << busData.address << " " << (int)busData.writeback << endl;
        // Check if the stall counter has reached zero
        if (busData.stalls == 0)
        {
            total_bus_traffic_bytes += caches[busData.coreId].blockSize; // Increment bus traffic counter
            int core = busData.coreId;
            int addr = busData.address;
            bool isWrite = busData.write;
            bool isWriteback = busData.writeback;
            data_traffic_bytes[core] += caches[core].blockSize; // Add writeback traffic
            // Process completed bus data transfer
            if (!isWriteback)
            {

                // Extract index and tag fields from the address
                int index = (addr >> b) & ((1 << s) - 1);
                int tag = addr >> (s + b);
                // Process read or write miss
                if (isWrite)
                {
                    int way = handle_write_miss(core, index, tag);
                    mesiState[core][index][way] = MESIState::M; // Set to Modified state
                }
                else
                {
                    int way = handle_read_miss(core, index, tag);
                    // cout << core << " " << index << " " << tag << endl;
                    //  Check if other caches have the data to determine state
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
                        mesiState[core][index][way] = MESIState::S;
                    }
                    else
                    {
                        mesiState[core][index][way] = MESIState::E;
                        // cout << " " << "hi2" << core << " " << index << " " << tag << endl;
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
                writebacks[core]++; // Increment writeback counter
                // Handle writeback completion
                // Just clear stall flag as writeback is complete
                caches[core].stall = false;

                // Clear pending operation for this core
                corePendingOperation[core] = -1;
            }

            // Remove this entry from the queue

            busDataQueue.erase(busDataQueue.begin());
            if (busDataQueue.size() > 0)
            {
                // cout << "busDataQueue: " << busDataQueue.size() << endl;
            }
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