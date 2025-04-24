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

using namespace std;

// Define global variables declared as external in other files
int s = 2; // Default values, will be overridden by command line arguments
int b = 4;
int E = 2;

// Define global data structures
vector<BusReq> busQueue;
set<int> curr_write;
vector<BusData> busDataQueue;
vector<int> clockCycles;
vector<int> instructions;
Cache caches[4];
vector<vector<MESIState>> mesiState[4];

// Define trace vectors as global variables
vector<pair<char, const char *>> trace1;
vector<pair<char, const char *>> trace2;
vector<pair<char, const char *>> trace3;
vector<pair<char, const char *>> trace4;

vector<int> num_reads(4, 0);
vector<int> num_writes(4, 0);
vector<int> cache_misses(4, 0);
vector<int> cache_evictions(4, 0);
vector<int> writebacks(4, 0);
vector<int> bus_invalidations(4, 0);
vector<long long> data_traffic_bytes(4, 0);
int total_bus_transactions = 0;
long long total_bus_traffic_bytes = 0;

void simulateMulticore()
{
    // Initialize counters for trace position for each core
    vector<size_t> tracePos(4, 0);

    // References to the trace vectors for easier access
    const vector<vector<pair<char, const char *>>> traces = {
        trace1, trace2, trace3, trace4};

    // Track if each core has more instructions
    vector<bool> coreActive(4, true);

    // Main simulation loop
    bool simActive = true;
    int globalCycle = 0;

    while (simActive)
    {
        cout << globalCycle << endl;
        // Process each core in round-robin fashion
        for (int i = 0; i < 4; i++)
        {
            // Skip cores that have completed their trace
            if (!coreActive[i])
            {
                continue;
            }

            // Skip stalled cores without incrementing their position
            // Check if there are more instructions for this core
            if (tracePos[i] < traces[i].size())
            {
                // Get the current operation for this core
                pair<char, const char *> currentOp = traces[i][tracePos[i]];
                // Execute the operation
                run(currentOp, i);
            }
            else
            {
                coreActive[i] = false;
            }
        }

        bus();
        for (int i = 0; i < 4; i++)
        {

            if (!caches[i].stall && coreActive[i])
            {
                // Increment the trace position for the core if not stalled
                tracePos[i]++;
                instructions[i]++;
            }
        }
        // Check if simulation should continue
        simActive = false;
        for (int i = 0; i < 4; i++)
        {
            // Simulation continues if any core has more operations or is stalled
            if (coreActive[i] || caches[i].stall || !busDataQueue.empty())
            {
                simActive = true;
                break;
            }
        }

        globalCycle++;
    }

    // Print final statistics
    cout << "\n===== Simulation Results =====\n";
    cout << "Total simulation cycles: " << globalCycle - 1 << endl;

    for (int i = 0; i < 4; i++)
    {
        cout << "Core " << i << ":\n";
        cout << "  Instructions executed: " << instructions[i] << endl;
        cout << endl;
    }
    string trace_prefix = "app"; // Replace with tracefile from command-line if available
    int block_size = 1 << b;
    int num_sets = 1 << s;
    double cache_size_kb = (num_sets * E * block_size) / 1024.0;

    cout << "Simulation Parameters:\n";
    cout << "Trace Prefix: " << trace_prefix << "\n";
    cout << "Set Index Bits: " << s << "\n";
    cout << "Associativity: " << E << "\n";
    cout << "Block Bits: " << b << "\n";
    cout << "Block Size (Bytes): " << block_size << "\n";
    cout << "Number of Sets: " << num_sets << "\n";
    cout << fixed << setprecision(2) << "Cache Size (KB per core): " << cache_size_kb << "\n";
    cout << "MESI Protocol: Enabled\n";
    cout << "Write Policy: Write-back, Write-allocate\n";
    cout << "Replacement Policy: LRU\n";
    cout << "Bus: Central snooping bus\n\n";

    for (int i = 0; i < 4; i++)
    {
        cout << "Core " << i << " Statistics:\n";
        cout << "Total Instructions: " << instructions[i] << "\n";
        cout << "Total Reads: " << num_reads[i] << "\n";
        cout << "Total Writes: " << num_writes[i] << "\n";
        cout << "Total Execution Cycles: " << clockCycles[i] << "\n";
        cout << "Idle Cycles: " << (globalCycle - 1 - clockCycles[i]) << "\n";
        cout << "Cache Misses: " << cache_misses[i] << "\n";
        double miss_rate = (num_reads[i] + num_writes[i] > 0) ? (cache_misses[i] * 100.0) / (num_reads[i] + num_writes[i]) : 0.0;
        cout << fixed << setprecision(2) << "Cache Miss Rate: " << miss_rate << "%\n";
        cout << "Cache Evictions: " << cache_evictions[i] << "\n";
        cout << "Writebacks: " << writebacks[i] << "\n";
        cout << "Bus Invalidations: " << bus_invalidations[i] << "\n";
        cout << "Data Traffic (Bytes): " << data_traffic_bytes[i] << "\n\n";
    }

    cout << "Overall Bus Summary:\n";
    cout << "Total Bus Transactions: " << total_bus_transactions << "\n";
    cout << "Total Bus Traffic (Bytes): " << total_bus_traffic_bytes << "\n";
}

void printUsage(const char *progName)
{
    cout << "Usage: " << progName << " -t <tracefile> -s <s> -E <E> -b <b> [-o <outfilename>] [-h]\n"
         << "\nOptions:\n"
         << "  -t <tracefile>  Name of the parallel application (e.g. app1) whose 4 traces are\n"
         << "                  to be used in simulation.\n"
         << "  -s <s>          Number of set index bits (number of sets in the cache = S = 2^s).\n"
         << "  -E <E>          Associativity (number of cache lines per set).\n"
         << "  -b <b>          Number of block bits (block size = B = 2^b).\n"
         << "  -o <outfilename>Log output in file for plotting etc.\n"
         << "  -h              Print this help message.\n";
}

int main(int argc, char *argv[])
{
    string tracefile;
    string outfilename;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0)
        {
            printUsage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "-t") == 0)
        {
            if (i + 1 < argc)
            {
                tracefile = argv[++i];
            }
            else
            {
                cerr << "Error: Missing argument for -t option.\n";
                return 1;
            }
        }
        else if (strcmp(argv[i], "-s") == 0)
        {
            if (i + 1 < argc)
            {
                s = atoi(argv[++i]);
            }
            else
            {
                cerr << "Error: Missing argument for -s option.\n";
                return 1;
            }
        }
        else if (strcmp(argv[i], "-E") == 0)
        {
            if (i + 1 < argc)
            {
                E = atoi(argv[++i]);
            }
            else
            {
                cerr << "Error: Missing argument for -E option.\n";
                return 1;
            }
        }
        else if (strcmp(argv[i], "-b") == 0)
        {
            if (i + 1 < argc)
            {
                b = atoi(argv[++i]);
            }
            else
            {
                cerr << "Error: Missing argument for -b option.\n";
                return 1;
            }
        }
        else if (strcmp(argv[i], "-o") == 0)
        {
            if (i + 1 < argc)
            {
                outfilename = argv[++i];
            }
            else
            {
                cerr << "Error: Missing argument for -o option.\n";
                return 1;
            }
        }
        else
        {
            cerr << "Error: Unknown option " << argv[i] << ".\n";
            return 1;
        }
    }

    // Initialize caches and MESI state vectors
    for (int i = 0; i < 4; ++i)
    {
        caches[i].init();
        mesiState[i].assign(1 << s, vector<MESIState>(E, MESIState::I));
    }

    // Initialize the trace vectors with test data
    trace1 = {{'W', "0x1"}, {'R', "0x3"}, {'R', "0x1"}}; // Core 0 reads from address 0x0
    trace2 = {{'W', "0x2"}};                             // Core 1 reads from address 0x0

    // trace3 = {{'W', "0x1"}}; // Core 1 reads from address 0x0
    //  Other cores have no operations initially

    // Initialize simulation counters
    instructions.assign(4, 0);
    clockCycles.assign(4, 0);

    // Run the simulation
    simulateMulticore();

    return 0;
}