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
int s = 2;  // Default values, will be overridden by command line arguments
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
vector<pair<char, const char*>> trace1;
vector<pair<char, const char*>> trace2;
vector<pair<char, const char*>> trace3;
vector<pair<char, const char*>> trace4;

void simulateMulticore() {
    // Initialize counters for trace position for each core
    vector<size_t> tracePos(4, 0);
    
    // References to the trace vectors for easier access
    const vector<vector<pair<char, const char*>>> traces = {
        trace1, trace2, trace3, trace4
    };
    
    // Track if each core has more instructions
    vector<bool> coreActive(4, true);
    
    // Main simulation loop
    bool simActive = true;
    int globalCycle = 0;
    
    while (simActive) {
        // Process bus operations first to handle any pending transfers
        bus();
        
        // Process each core in round-robin fashion
        for (int i = 0; i < 4; i++) {
            // Skip cores that have completed their trace or are stalled
            if (!coreActive[i] || caches[i].stall) {
                continue;
            }
            
            // Check if there are more instructions for this core
            if (tracePos[i] < traces[i].size()) {
                // Get the next operation for this core
                pair<char, const char*> nextOp = traces[i][tracePos[i]];
                
                // Execute the operation
                run(nextOp, i);
                
                // Move to next instruction
                tracePos[i]++;
                instructions[i]++;
                
                // Check if we've reached the end of the trace
                if (tracePos[i] >= traces[i].size()) {
                    coreActive[i] = false;
                }
            } else {
                coreActive[i] = false;
            }
        }
        
        // Check if simulation should continue
        simActive = false;
        for (int i = 0; i < 4; i++) {
            // Simulation continues if any core has more operations or is stalled
            if (coreActive[i] || caches[i].stall || !busDataQueue.empty()) {
                simActive = true;
                break;
            }
        }
        
        globalCycle++;
        
        // Optional: Print progress or status every N cycles
        if (globalCycle % 1000 == 0) {
            cout << "Simulation cycle: " << globalCycle << endl;
        }
    }
    
    // Print final statistics
    cout << "\n===== Simulation Results =====\n";
    cout << "Total simulation cycles: " << globalCycle << endl;
    
    for (int i = 0; i < 4; i++) {
        cout << "Core " << i << ":\n";
        cout << "  Instructions executed: " << instructions[i] << endl;
        cout << endl;
    }
}

void printUsage(const char* progName) {
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

int main(int argc, char *argv[]) {
    string tracefile;
    string outfilename;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            printUsage(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "-t") == 0) {
            if (i + 1 < argc) {
                tracefile = argv[++i];
            }
            else {
                cerr << "Error: Missing argument for -t option.\n";
                return 1;
            }
        }
        else if (strcmp(argv[i], "-s") == 0) {
            if (i + 1 < argc) {
                s = atoi(argv[++i]);
            }
            else {
                cerr << "Error: Missing argument for -s option.\n";
                return 1;
            }
        }
        else if (strcmp(argv[i], "-E") == 0) {
            if (i + 1 < argc) {
                E = atoi(argv[++i]);
            }
            else {
                cerr << "Error: Missing argument for -E option.\n";
                return 1;
            }
        }
        else if (strcmp(argv[i], "-b") == 0) {
            if (i + 1 < argc) {
                b = atoi(argv[++i]);
            }
            else {
                cerr << "Error: Missing argument for -b option.\n";
                return 1;
            }
        }
        else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                outfilename = argv[++i];
            }
            else {
                cerr << "Error: Missing argument for -o option.\n";
                return 1;
            }
        }
        else {
            cerr << "Error: Unknown option " << argv[i] << ".\n";
            return 1;
        }
    }
    
    // Initialize caches and MESI state vectors
    for (int i = 0; i < 4; ++i) {
        caches[i].init();
        mesiState[i].assign(1 << s, vector<MESIState>(E, MESIState::I));
    }

    // Initialize the trace vectors with test data
    trace1 = {{'W', "0x1"}}; // Core 0 reads from address 0x0
    trace2 = {{'R', "0x1"}}; // Core 0 reads from address 0x0
    // Other cores have no operations initially
    
    // Initialize simulation counters
    instructions.assign(4, 0);
    clockCycles.assign(4, 0);

    // Run the simulation
    simulateMulticore();
    
    return 0;
}