#include "defines.h"
#include "logger.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <switch.h>

namespace SbbLog {
    std::string Logger::getCurrentDate() {
        time_t now = 0;
        Result res = timeGetCurrentTime(TimeType_UserSystemClock, (u64*)&now);
        if (R_FAILED(res)) {
            now = std::time(nullptr);
        }

        std::tm* localTime = std::localtime(&now);
        std::ostringstream oss;
        oss << std::put_time(localTime, "%Y-%m-%d");
        return oss.str();
    }

    std::string Logger::getCurrentTimestamp() {
        time_t now = 0;
        Result res = timeGetCurrentTime(TimeType_UserSystemClock, (u64*)&now);
        if (R_FAILED(res)) {
            now = std::time(nullptr);
        }

        std::tm* localTime = std::localtime(&now);
        std::ostringstream oss;
        oss << std::put_time(localTime, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    void Logger::logToFile(const std::string& message) {
        std::string date = getCurrentDate();
        std::string filename = "sdmc:/atmosphere/contents/430000000000000B/log_" + date + ".txt";
        std::ofstream logFile(filename, std::ios::app);
        if (logFile.is_open()) {
            logFile << "[" << getCurrentTimestamp() << "] " << message << std::endl;
            logFile.close();
        }
    }
}
