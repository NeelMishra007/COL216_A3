#ifndef BUS_HPP
#define BUS_HPP

#include <vector>
using namespace std;
// Enum for different types of bus transactions in MESI protocol
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

extern vector<BusReq> busQueue;
#endif // BUS_HPP
