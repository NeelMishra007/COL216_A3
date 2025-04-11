#include <iostream>      
#include <string>        
#include <vector>      
#include <sstream>      
#include <iomanip>       
#include <algorithm>     
#include <cstring>       
#include <cstdlib>  
#include "main.hpp"   

using namespace std;

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
    for (int i = 0; i < 4; ++i) {
        caches[i].init();
        mesiState[i].assign(1 << s, vector<MESIState>(E, MESIState::I));
    }

}