#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <fstream>
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

// Function to load trace files based on prefix
bool loadTraceFiles(const string &tracePrefix)
{
    vector<vector<pair<char, const char *>> *> traces = {&trace1, &trace2, &trace3, &trace4};

    for (int i = 0; i < 4; i++)
    {
        // Construct filename: app1_proc0.trace, app1_proc1.trace, etc.
        string filename = tracePrefix + "_proc" + to_string(i) + ".trace";
        ifstream traceFile(filename);

        if (!traceFile.is_open())
        {
            cerr << "Error: Could not open trace file " << filename << endl;
            return false;
        }

        // Clear any existing data
        traces[i]->clear();

        string line;
        while (getline(traceFile, line))
        {
            // Skip empty lines or comments
            if (line.empty() || line[0] == '#')
            {
                continue;
            }

            // Parse line like "R 0x817b08"
            istringstream iss(line);
            char op;
            string address;

            if (iss >> op >> address)
            {
                // Only process read (R) and write (W) operations
                if (op == 'R' || op == 'W')
                {
                    // We need to create a persistent copy of the address string
                    char *addressCopy = new char[address.length() + 1];
                    strcpy(addressCopy, address.c_str());
                    traces[i]->push_back({op, addressCopy});
                }
            }
        }

        traceFile.close();
    }

    return true;
}

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
        if (globalCycle % 100000 == 0)
        {
            cout << coreActive[0] << " " << coreActive[1] << " " << coreActive[2] << " " << coreActive[3] << endl;
        }
        // cout << coreActive[0] << " " << coreActive[1] << " " << coreActive[2] << " " << coreActive[3] << endl;
        // cout << globalCycle << endl;
        //  Process each core in round-robin fashion
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
                // if (globalCycle % 100000 == 0)
                // {
                //     cout << "Core " << i << " Cycle: " << globalCycle << ", Instruction: " << tracePos[i] << endl;
                // }
                run(currentOp, i);
            }
            else
            {
                coreActive[i] = false;
                cout << i << endl;
            }
        }

        bus();
        for (int i = 0; i < 4; i++)
        {

            if (!caches[i].stall && coreActive[i])
            {
                if (tracePos[i] % 100000 == 0)
                {
                    cout << "Core " << i << " Cycle: " << globalCycle << ", Instruction: " << tracePos[i] << endl;
                }
                // cout << i << " " << tracePos[i] << endl;
                tracePos[i]++;
                instructions[i]++;
                if (tracePos[i] == traces[i].size())
                {
                    clockCycles[i] = globalCycle;
                }
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

    for (int i = 0; i < 4; i++)
    {
        // Update the clock cycles for each core
        for (int j = 0; j < traces[i].size(); j++)
        {
            if (traces[i][j].first == 'R')
            {
                num_reads[i]++;
            }
            else if (traces[i][j].first == 'W')
            {
                num_writes[i]++;
            }
        }
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
    string tracePrefix;
    string outfilename;

    // Parse command line arguments
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
                tracePrefix = argv[++i];
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
            cout << "Output file name: " << argv[i] << endl;
        }
        else
        {
            cerr << "Error: Unknown option " << argv[i] << ".\n";
            return 1;
        }
    }

    // Check if required arguments are provided
    if (tracePrefix.empty())
    {
        cerr << "Error: Trace file prefix (-t) is required.\n";
        printUsage(argv[0]);
        return 1;
    }

    // Load trace files
    if (!loadTraceFiles(tracePrefix))
    {
        cerr << "Error loading trace files. Exiting.\n";
        return 1;
    }

    trace1 = {{'W', "0x0"}, {'R', "0x3"}};
    trace2 = {{'W', "0x0"}};
    trace3 = {};
    trace4 = {};
    // Initialize caches and MESI state vectors
    for (int i = 0; i < 4; ++i)
    {
        caches[i].init();
        mesiState[i].assign(1 << s, vector<MESIState>(E, MESIState::I));
    }

    // Initialize simulation counters
    instructions.assign(4, 0);
    clockCycles.assign(4, 0);

    // Set up output file if specified
    ofstream outFile;
    if (!outfilename.empty())
    {
        outFile.open(outfilename);
        if (!outFile.is_open())
        {
            cerr << "Error: Could not open output file " << outfilename << endl;
            return 1;
        }
        // Redirect cout to the file
        streambuf *coutBuffer = cout.rdbuf();
        cout.rdbuf(outFile.rdbuf());

        // Run simulation
        simulateMulticore();

        // Restore cout
        cout.rdbuf(coutBuffer);
        outFile.close();
    }
    else
    {
        // Run simulation with output to console
        simulateMulticore();
    }

    // Clean up dynamically allocated memory for address strings
    for (auto &p : trace1)
        delete[] p.second;
    for (auto &p : trace2)
        delete[] p.second;
    for (auto &p : trace3)
        delete[] p.second;
    for (auto &p : trace4)
        delete[] p.second;

    return 0;
}