#ifndef BUS_HPP
#define BUS_HPP

#include <vector>
#include <set>
using namespace std;
// Enum for different types of bus transactions in MESI protocol

void bus();
enum class BusReqType {
    BusRd,     // Read from memory (shared or modified line needed)
    BusRdX,    // Read for exclusive (write) — others must invalidate
    BusUpgr    // Upgrade from shared to exclusive (intent to write)
};

// Struct to represent a bus transaction
struct BusReq {
    int coreId;                 // ID of the core making the request
    int address;           // 32-bit memory address
    BusReqType type;            // Type of bus request
};

struct BusData {
    int address;        // 32-bit memory address of the cache line
    int coreId;      // ID of the core receiving the data
    bool write;            // Read or write operation
    bool writeback;       // Indicates if the data is being written back to memory
    int stalls;           // Number of stalls for the bus transaction
};

extern vector<BusReq> busQueue;
extern set<int> curr_write;
extern vector<BusData> busDataQueue;
#endif // BUS_HPP
