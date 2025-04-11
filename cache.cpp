#include <iostream>      
#include <string>        
#include <vector>      
#include <sstream>      
#include <iomanip>       
#include <algorithm>     
#include <cstring>       
#include <cstdlib>     

using namespace std;

unsigned int parseHexAddress(const string& address) {
    string hexPart = address;
    
    // Remove "0x" prefix if present
    if (hexPart.length() >= 2 && (hexPart.substr(0, 2) == "0x" || hexPart.substr(0, 2) == "0X")) {
        hexPart = hexPart.substr(2);
    }
    
    // Convert hex string to integer
    unsigned int addr_val = stoul(hexPart, nullptr, 16);
    
    // Format with leading zeros using stringstream (for debug output)
    stringstream ss;
    ss << "0x" << setfill('0') << setw(8) << hex << addr_val;
    string paddedAddress = ss.str();
    
    // Display the padded address (consider making this optional or removing for production)
    cout << "Padded Address: " << paddedAddress << endl;
    
    return addr_val;
}
void runSimulation(const vector<pair<char, const char*>>& trace, int s, int E, int b) {
    int sets = 1 << s;                        // Number of sets = 2^s
    int blockSize = 1 << b;                     // Block size = 2^b bytes
    
    vector<vector<unsigned int>> tags(sets, vector<unsigned int>(E, 0));  // Tags
    vector<vector<bool>> valid(sets, vector<bool>(E, false));             // Valid bits
    vector<vector<int>> lru(sets);                // LRU counters (initialize as empty vectors)
    vector<vector<bool>> dirty(sets, vector<bool>(E, false));             
    
    for (int i = 0; i < sets; i++) {
        for (int j = 0; j < E; j++) {
            lru[i].push_back(j);
        }
    }
    
    for (const auto &entry : trace) {
        char accessType = entry.first;
        string address = entry.second;
    
        unsigned int addr = parseHexAddress(address);
    
        unsigned int index = (addr >> b) & ((1 << s) - 1); // Extract index bits
        unsigned int tag = addr >> (s + b);                // Extract tag bits
        bool hit = false;
        int hit_line = -1;
    
        if (accessType == 'R') {
            for (int i = 0; i < E; i++) {
                if (valid[index][i] && tags[index][i] == tag) {
                    hit = true;
                    hit_line = i;
                    break;
                }
            }
            if (hit) {
                auto it = find(lru[index].begin(), lru[index].end(), hit_line);
                if (it != lru[index].end()) {
                    lru[index].erase(it);
                }
                lru[index].push_back(hit_line); // Update LRU
            }
            else {
                // Read miss: try to find an invalid block first.
                int target_line = -1;
                for (int i = 0; i < E; i++) {
                    if (!valid[index][i]) {
                        target_line = i;
                        break;
                    }
                }
                // If no invalid block is available, choose the LRU block (front of vector)
                if (target_line == -1) {
                    target_line = lru[index].front();
                    lru[index].erase(lru[index].begin());
                    // If the block is dirty, write it back before replacement.
                    if (dirty[index][target_line]) {
                        // Write-back logic here (e.g., write block back to memory)
                    }
                }
                else {
                    // Remove the chosen block from its current position in LRU.
                    auto it = find(lru[index].begin(), lru[index].end(), target_line);
                    if (it != lru[index].end()) {
                        lru[index].erase(it);
                    }
                }
        
                // Load the block from memory and update cache metadata.
                tags[index][target_line] = tag;
                valid[index][target_line] = true;
                dirty[index][target_line] = false; // Read miss: block is not dirty
                // Update LRU: mark the newly loaded block as most recently used.
                lru[index].push_back(target_line);
            }        
        }
        else {
            for (int i = 0; i < E; i++) {
                if (valid[index][i] && tags[index][i] == tag) {
                    hit = true;
                    hit_line = i;
                    break;
                }
            }
            if (hit) {
                auto it = find(lru[index].begin(), lru[index].end(), hit_line);
                if (it != lru[index].end()) {
                    lru[index].erase(it);
                }
                lru[index].push_back(hit_line); // Update LRU
                dirty[index][hit_line] = true; // Mark the block as dirty
            }
            else {
                // Write miss: try to find an invalid block first.
                int target_line = -1;
                for (int i = 0; i < E; i++) {
                    if (!valid[index][i]) {
                        target_line = i;
                        break;
                    }
                }
                // If no invalid block is available, choose the LRU block (front of vector)
                if (target_line == -1) {
                    target_line = lru[index].front();
                    lru[index].erase(lru[index].begin());
                    // If the block is dirty, write it back before replacement.
                    if (dirty[index][target_line]) {
                        // Write-back logic here (e.g., write block back to memory)
                    }
                }
                else {
                    // Remove the chosen block from its current position in LRU.
                    auto it = find(lru[index].begin(), lru[index].end(), target_line);
                    if (it != lru[index].end()) {
                        lru[index].erase(it);
                    }
                }
        
                // Load the block from memory and update cache metadata.
                tags[index][target_line] = tag;
                valid[index][target_line] = true;
                dirty[index][target_line] = true; // Write miss: block is dirty
                // Update LRU: mark the newly loaded block as most recently used.
                lru[index].push_back(target_line);
            }

        }
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
    int s = 0;
    int E = 0;
    int b = 0;

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
    vector<pair<char, const char*>> trace = { { 'R', "0xf0a3b28" } };
    
    runSimulation(trace, s, E, b);

}