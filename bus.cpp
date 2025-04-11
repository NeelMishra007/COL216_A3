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

    while (busQueue.size()) {
        BusReq busReq = busQueue.front();
        busQueue.erase(busQueue.begin());

        int core = busReq.coreId;
        int addr = busReq.address;
        BusReqType type = busReq.type;

        int index = (addr >> b) & ((1 << s) - 1);
        int tag = addr >> (s + b);                  

        if (type == BusReqType::BusRd) {
            bool found = false;
            for (int i = 0; i < 4; i++) {
                if (i != core) {
                    for (int j = 0; j < E; j++) {
                        if (caches[i].tags[index][j] == tag && mesiState[i][index][j] != MESIState::I) {
                            found = true;

                            if (mesiState[i][index][j] == MESIState::M) {
                                // Send BusRd to share the line with the requesting core.
                                mesiState[i][index][j] == MESIState::S;
                                caches[i].stalls += 100; //Write back to memory
                            } else if (mesiState[i][index][j] == MESIState::E) {
                                // Send BusRd to share the line with the requesting core.
                                mesiState[i][index][j] == MESIState::S;
                            }
                            mesiState[core][index][j] == MESIState::S;
                            caches[core].stalls += 1 << (b - 1);
                            break;
                        }
                    }
                }
                if (found) {
                    break;
                }
            }
            if (!found) {
                caches[core].stalls += 100;
                mesiState[core][index][0] = MESIState::E; // Update the state to exclusive.
            }
            handle_read_miss(core, index, tag); // Handle the read miss in the cache.

        } 
        else if (type == BusReqType::BusUpgr) {
            

        }
        else if (type == BusReqType::BusRdX) {

        } 
    }
}