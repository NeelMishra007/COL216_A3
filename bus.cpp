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

vector<int> corePendingOperation(4, -1); // -1 indicates no pending operation
bool bus_busy = false;
int cycle = 0;
int cnt = 0;
void bus()
{
    cycle++;
    while (busQueue.size())
    {
        BusReq busReq = busQueue.front();
        busQueue.erase(busQueue.begin());

        if (cycle % 100000 == 0)
        {
            // cout << "Bus: " << busReq.coreId << " " << busReq.address << " " << (int)busReq.type << endl;
        }
        // cout << bus_busy << endl;
        int core = busReq.coreId;
        int addr = busReq.address;
        BusReqType type = busReq.type;

        int index = (addr >> b) & ((1 << s) - 1);
        int tag = addr >> (s + b);

        if (bus_busy)
        {
            caches[core].stall = true;
            idle_cycles[core]++;
            continue;
        }
        corePendingOperation[core] = addr;
        if (type == BusReqType::BusRd)
        {
            bus_busy = true;
            total_bus_transactions++; // Increment bus transaction counter
            cache_misses[core]++;     // Increment miss counter
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
                            caches[core].stall = true;                                                      // Set the stall flag for the requesting core
                            busDataQueue.push_back(BusData{addr, core, false, false, false, 1 << (b - 1)}); // Send data to the requesting core
                            data_traffic_bytes[i] += caches[i].blockSize;
                            if (mesiState[i][index][j] == MESIState::M)
                            {
                                // Send BusRd to share the line with the requesting core
                                mesiState[i][index][j] = MESIState::S;
                                caches[i].stall = true;                                            // Set the stall flag for the core
                                busDataQueue.push_back(BusData{addr, i, false, true, false, 100}); // Writeback data
                                if (coreActive[i])
                                {
                                    clockCycles[i] -= ((1 << (b - 1)) + 101);
                                    idle_cycles[i] += (1 << (b - 1)) + 1;
                                }
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
                busDataQueue.push_back(BusData{addr, core, false, false, false, 100}); // Send data to the requesting core
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
                                caches[i].stall = true;                                            // Set the stall flag for the core
                                busDataQueue.push_back(BusData{addr, i, false, true, false, 100}); // Writeback data
                                if (coreActive[i])
                                    clockCycles[i] -= 101;
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
            busDataQueue.push_back(BusData{addr, core, true, false, false, 100});
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
                                // bus_invalidations[i]++;                // Increment invalidation counter
                            }
                        }
                    }
                }

                // Upgrade the state to Modified
                bus_invalidations[core]++; // Increment invalidation counter
                bus_busy = true;
                mesiState[core][index][target_line] = MESIState::M;
                caches[core].dirty[index][target_line] = true; // Mark the line as dirty
                caches[core].stall = true;
                busDataQueue.push_back(BusData{addr, core, false, false, true, 0}); // Inv
                corePendingOperation[core] = 1;
            }
        }
    }
    // Handle a single bus data operation if any
    if (!busDataQueue.empty())
    {
        BusData &busData = busDataQueue.front();
        if (busData.stalls == 0)
        {
            total_bus_traffic_bytes += caches[busData.coreId].blockSize; // Increment bus traffic counter
            int core = busData.coreId;
            int addr = busData.address;
            bool isWrite = busData.write;
            bool isWriteback = busData.writeback;
            bool inv = busData.inv;
            data_traffic_bytes[core] += caches[core].blockSize; // Add writeback traffic
            bool evictwriteback = false;
            if (!isWriteback)
            {
                int index = (addr >> b) & ((1 << s) - 1);
                int tag = addr >> (s + b);
                if (isWrite)
                {
                    int way = handle_write_miss(core, index, tag, evictwriteback);
                    mesiState[core][index][way] = MESIState::M; // Set to Modified state
                }
                else if (!inv)
                {
                    int way = handle_read_miss(core, index, tag, evictwriteback);
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
                    if (otherCachesHaveData)
                    {
                        mesiState[core][index][way] = MESIState::S;
                    }
                    else
                    {

                        mesiState[core][index][way] = MESIState::E;
                    }
                }
                caches[core].stall = false;
                // clockCycles[core]++;
                corePendingOperation[core] = -1;
                if (evictwriteback)
                {
                    caches[core].stall = true;
                    corePendingOperation[core] = 1;
                }
            }
            else
            {
                writebacks[core]++;
                caches[core].stall = false;
                corePendingOperation[core] = -1;
            }

            busDataQueue.erase(busDataQueue.begin());
            if (busDataQueue.empty())
            {
                bus_busy = false;
            }
        }
        else
        {
            busData.stalls--;
        }
    }
}