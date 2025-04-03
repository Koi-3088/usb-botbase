#include <iostream>
#include <fstream>
#include "log.h"
#include "defines.h"

namespace SbbLog {
    // Add creating a file, and max line length per file per day. Add std debug output?
    void Logger::logToFile(const std::string& message, const std::string& filename) {
        std::ofstream logFile;
        logFile.open(filename, std::ios::app);
        if (logFile.is_open()) {
            logFile << message << std::endl;
            logFile.close();
        }
        else {
            return;
        }
    }
}
