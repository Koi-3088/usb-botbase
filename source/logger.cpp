#include "defines.h"
#include "logger.h"
#include <fstream>
#include <iostream>

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
