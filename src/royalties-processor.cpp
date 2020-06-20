#include <iostream> // istream, ostream
#include <fstream> // ifstream, ofstream
#include <vector>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <windows.h> // QueryPerformanceCounter, LARGE_INTEGER

// super simple logging function
namespace log {
    enum class level {
        INFO, WARNING, FATAL
    };
    void log(std::ostream& out, std::string m, level l) {
        std::string line;
        switch(l) {
            case level::INFO:
                line += "[INFO] ";
                break;
            
            case level::WARNING:
                line += "[WARNING] ";
                break;
            
            case level::FATAL:
                line += "[FATAL] ";
                break;
        }
        line += m;
        out << line << std::endl;
    }
}

//  configurable "constants" used by the main program, plus a way to read/write a config file
namespace config {
    // defaults
    std::string WRITERS = "WRITERS";
    std::string RIGHT_TYPE_GROUP = "RIGHT_TYPE_GROUP";
    std::string FINAL_DISTRIBUTED_AMOUNT = "FINAL_DISTRIBUTED_AMOUNT";
    std::string DISTRIBUTED_AMOUNT = "DISTRIBUTED_AMOUNT";
    double COEFFICIENT_PERF=0.6, COEFFICIENT_MECH=0.8, COEFFICIENT_SYNC=0.7, COEFFICIENT_PRINT=0.8;

    void readConfig(std::istream& in) {
        std::string line, key, value;
        std::stringstream lineSs;
        while(in.good()) {
            std::getline(in, line);
            lineSs.str(line);
            if(line.size()) {
                // allow simple comments
                if(line[0]=='#') continue;


                std::getline(lineSs, key, '=');
                if(key=="WRITERS") {
                    lineSs >> WRITERS;
                } else if(key=="RIGHT_TYPE_GROUP") {
                    lineSs >> RIGHT_TYPE_GROUP;
                } else if(key=="FINAL_DISTRIBUTED_AMOUNT") {
                    lineSs >> FINAL_DISTRIBUTED_AMOUNT;
                } else if(key=="DISTRIBUTED_AMOUNT") {
                    lineSs >> DISTRIBUTED_AMOUNT;
                } else if(key=="COEFFICIENT_PERF") {
                    lineSs >> COEFFICIENT_PERF;
                } else if(key=="COEFFICIENT_MECH") {
                    lineSs >> COEFFICIENT_MECH;
                } else if(key=="COEFFICIENT_SYNC") {
                    lineSs >> COEFFICIENT_SYNC;
                } else if(key=="COEFFICIENT_PRINT") {
                    lineSs >> COEFFICIENT_PRINT;
                }
            }
        }
    }
    void writeConfig(std::ostream& out) {
        out << "WRITERS=" << WRITERS << std::endl;
        out << "RIGHT_TYPE_GROUP=" << RIGHT_TYPE_GROUP << std::endl;
        out << "FINAL_DISTRIBUTED_AMOUNT=" << FINAL_DISTRIBUTED_AMOUNT << std::endl;
        out << "DISTRIBUTED_AMOUNT=" << DISTRIBUTED_AMOUNT << std::endl;
        out << "COEFFICIENT_PERF=" << COEFFICIENT_PERF << std::endl;
        out << "COEFFICIENT_MECH=" << COEFFICIENT_MECH << std::endl;
        out << "COEFFICIENT_SYNC=" << COEFFICIENT_SYNC << std::endl;
        out << "COEFFICIENT_PRINT=" << COEFFICIENT_PRINT << std::endl;
    }
}

//TODO create an iterator to replace read/write functions
// helper function to read in a CSV line, supporting quoted cells
std::vector<std::string> readCsvLine(std::istream& str) {
    std::vector<std::string> result;

    std::string line, cell, cellnq;
    std::stringstream lineStream, cellStream;
    std::getline(str, line);
    lineStream << line;

    bool inCell = false;
    for(char c : line) {
        switch(c) {
            case '"': // if cells are wholly contained in single quotes, disable comma checking until end of quote
                inCell = !inCell;
                break;
            case ',':
                if(!inCell) {
                    result.push_back(cellStream.str());
                    cellStream.str("");
                    cellStream.clear();
                } else cellStream << c;
                break;
            default:
                cellStream << c;
        }
    }

    return result;
}

// write a line of CSV, with each cell encased in quotes
void writeCsvLine(std::ostream& out, const std::vector<std::string>& line) {
    for(auto cell : line) {
        out << "\"" << cell << "\",";
    }
    out << "\n";
}

// helper function to split a string, since writer column is split by semicolons
std::vector<std::string> split(std::string const& str, std::string delim = "; ") {
    std::vector<std::string> result;
    auto start = 0U;
    auto end = str.find(delim);
    while(end != std::string::npos) {
        result.push_back(str.substr(start, end - start));
        start = end + delim.size();
        end = str.find(delim, start);
    }
    result.push_back(str.substr(start, str.size() - start));

    return result;
}

int main() {
    // MANAGE CONFIG
    std::cout << "Use default config? (Y/N): ";
    bool useDefault = true;
    bool configDecided = false;
    std::string configDecision;
    while(!configDecided) {
        std::cin >> configDecision;
        std::cin.ignore();
        switch(std::tolower(configDecision[0])) {
            case 'y':
                configDecided = true;
                useDefault = true;
                break;
            
            case 'n':
                configDecided = true;
                useDefault = false;
                break;
        }
    }

    if(!useDefault) {
        std::ifstream cfgIn("config.cfg");
        if(!cfgIn.is_open()) {
            log::log(std::cout, "Config file not found, writing new config...", log::level::WARNING);
            std::ofstream cfgOut("config.cfg");
            config::writeConfig(cfgOut);
            cfgOut.close();
        } else {
            config::readConfig(cfgIn);
            cfgIn.close();
        }
    }


    // OPEN MASTER FILE
    std::cout << "Enter name of master CSV file (not including extension): " << std::endl;
    std::string masterFile;
    std::getline(std::cin, masterFile);
    // std::cin>>std::ws;
    std::ifstream master(masterFile + ".csv");
    if(!master.is_open()) {
        log::log(std::cout, "Failed to open master file \"" + masterFile + "\"!", log::level::FATAL);
        return 1;
    }

    // PROCESS MASTER FILE

    // read header line
    std::vector<std::string> header = readCsvLine(master);

    // get index of columns to process 
    std::size_t idWriters, idType, idFinalAmount, idInitialAmount;
    bool createFinalColumn = true;
    for(std::size_t i=0; i<header.size(); i++) {
        if(header[i] == config::WRITERS) idWriters=i;
        else if(header[i] == config::RIGHT_TYPE_GROUP) idType=i;
        else if(header[i] == config::DISTRIBUTED_AMOUNT) idInitialAmount = i;
        else if(header[i] == config::FINAL_DISTRIBUTED_AMOUNT) {
            idFinalAmount=i;
            createFinalColumn=false;
        }
    }
    if(createFinalColumn) { // will need to create new column for final amount in output files
        log::log(std::cout, "No Final Distributed Amount column found in input. This column will be created in the output files.", log::level::INFO);
    }

    // read in list of writers
    std::unordered_map<std::string, std::unordered_set<unsigned int>> writers;
    std::vector<std::string> tmpLine, lineWriters;
    for(unsigned int i=0; !master.eof(); i++) {
        tmpLine = readCsvLine(master);
        if(tmpLine.size() > idWriters) { // avoid segfault
            lineWriters = split(tmpLine[idWriters]);
            for(auto w : lineWriters) {
                // insert if unique
                writers.emplace(w, std::unordered_set<unsigned int>()).first->second.emplace(i);
            }
        }
    }

    // PROCESS WRITERS
    double totalFDR = 0;
    int inputWriterId;
    std::vector<int> inputWriterIds;
    std::unordered_map<int, std::string> writerIdMap;
    std::string outputFileName;
    std::ofstream fout;

    std::cout << "WRITERS:" << std::endl;
    int count = 1;
    for(auto w : writers) {
        writerIdMap.emplace(count, w.first);
        std::cout << "[" << count << "] " << w.first << "\n";
        count++;
    }
    std::cout << std::endl << "Please select the writers to include (space-separated IDs): " << std::endl;

    std::string inputIdStr;
    std::getline(std::cin, inputIdStr);
    // std::cin >> std::ws;
    std::stringstream inputIdSs(inputIdStr);
    while(inputIdSs >> inputWriterId) inputWriterIds.push_back(inputWriterId);

    std::cout << std::endl << "Please enter the name of the output file (not including extension): " << std::endl;
    std::getline(std::cin, outputFileName);
    outputFileName += ".csv";
    std::cout << std::endl;

    log::log(std::cout, "Processing...", log::level::INFO);

    //timer
    LARGE_INTEGER f, t1, t2;
    double elapsedTime;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t1);

    std::unordered_set<unsigned int> idsToGet;
    fout.open(outputFileName);
    if(fout.is_open()) {
        // add all line ids to working set
        for(auto w : inputWriterIds) {
            idsToGet.insert(writers[writerIdMap[w]].begin(), writers[writerIdMap[w]].end());
            
            log::log(std::cout, "Number of rows after adding writer \""+ writerIdMap[w] + "\": " + std::to_string(idsToGet.size()), log::level::INFO);
        }

        // get all needed lines
        // return ifstream to start
        master.clear();
        master.seekg(0);
        std::string lineStr;
        std::getline(master, lineStr); // skip header in input

        if(createFinalColumn) {
            idFinalAmount = idInitialAmount++; // assign then increment

            // fix header
            header.insert(header.cbegin() + idInitialAmount - 1, {config::FINAL_DISTRIBUTED_AMOUNT});
        }
        writeCsvLine(fout, header); // write [updated] header


        int writeCount = 0;
        for(int i=0; !idsToGet.empty(); i++) {
            if(idsToGet.count(i)>0) {
                tmpLine = readCsvLine(master);

                // add column if needed
                if(createFinalColumn) {
                    tmpLine.insert(tmpLine.cbegin() + idInitialAmount - 1, {""});
                }

                // manipulate value
                double val = std::stod(tmpLine[idInitialAmount]);
                if(tmpLine[idType] == "PERF") {
                    val*=config::COEFFICIENT_PERF;
                } else if(tmpLine[idType] == "MECH") {
                    val*=config::COEFFICIENT_MECH;
                } else if(tmpLine[idType] == "SYNCH") {
                    val*=config::COEFFICIENT_SYNC;
                } else if(tmpLine[idType] == "PRINT") {
                    val*=config::COEFFICIENT_PRINT;
                } else { // group not recognised
                    log::log(std::cout, "Found entry with unrecognised Right Type Group: " + tmpLine[idType] + ". Printing \"0\" as output.", log::level::WARNING);
                    val = 0;
                }

                tmpLine[idFinalAmount] = std::to_string(val);
                totalFDR += val;

                writeCsvLine(fout, tmpLine);
                idsToGet.erase(i);

                writeCount++;
            } else {
                std::getline(master, lineStr); // skip line
            }
        }
        log::log(std::cout, "File written successfully.", log::level::INFO);
        log::log(std::cout, "Number of rows written: " + std::to_string(writeCount), log::level::INFO);
    } else {
        log::log(std::cout, "Failed to write file", log::level::FATAL);
        return 0;
    }

    QueryPerformanceCounter(&t2);
    elapsedTime = (double) (t2.QuadPart - t1.QuadPart) / f.QuadPart;

    log::log(std::cout, "Elapsed Time: " + std::to_string(elapsedTime) + "s", log::level::INFO);

    std::cout << "Total Final Distributed Amount: " << totalFDR << std::endl;

    std::cout << "Press enter to close...";
    std::cin.get(); // pause
    return 0;
}
