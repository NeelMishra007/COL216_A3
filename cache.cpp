#include <iostream>
#include <cstdlib>    // For std::exit and std::atoi
#include <cstring>    // For strcmp
#include <string>
#include <vector>

using namespace std;

void runSimulation (vector<pair<char, const char*>> trace, int s, int E, int b) {
    vector<vector<int>> tag(1 << s, vector<int>(E, -1)); 
    vector<vector<bool>> valid(1 << s, vector<bool>(E, false));

    vector<vector<int>> lru(1 << s, vector<int>(E, 0)); 

    for (const auto &entry : trace) {
        char accessType = entry.first;
        const char* address = entry.second;

        // Remove "0x" or "0X" prefix if present.
        const char* addrWithoutPrefix = address;
        if (strncmp(address, "0x", 2) == 0 || strncmp(address, "0X", 2) == 0) {
            addrWithoutPrefix = address + 2;
        }

        // Convert the hexadecimal string to an unsigned integer using strtoul.
        unsigned int addr_val = (unsigned int)strtoul(addrWithoutPrefix, nullptr, 16);

        // Format the integer into an 8-digit hexadecimal string with leading zeros.
        // The output buffer needs to be at least 11 characters: "0x" + 8 hex digits + null terminator.
        char paddedAddress[11];
        sprintf(paddedAddress, "0x%08x", addr_val);

        // Debug: Print the access type and padded address.
        printf("Access: %c, Padded Address: %s\n", accessType, paddedAddress);

        // Here you would continue with the cache simulation logic.
        // For example, extract tag, set index, and block offset from paddedAddress.
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