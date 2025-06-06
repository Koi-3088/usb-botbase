#include "defines.h"
#include "logger.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <switch.h>
#include <sys\stat.h>
#include <chrono>

namespace SbbLog {
    size_t Logger::m_maxLogSize = 1024 * 1024 * 8;

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
        using namespace std::chrono;

        // Get seconds from system clock (required for correct time)
        u64 now_sec = 0;
        Result res = timeGetCurrentTime(TimeType_UserSystemClock, &now_sec);
        if (R_FAILED(res)) {
            Logger::logToFile("Failed to get current time", res);
            now_sec = static_cast<u64>(std::time(nullptr));
        }

        // Get microseconds from steady clock (relative, not absolute)
        auto now = system_clock::now();
        auto now_us = duration_cast<microseconds>(now.time_since_epoch()) % 1000000;

        // Convert seconds to tm
        time_t now_time_t = static_cast<std::time_t>(now_sec);
        tm* localTime = std::localtime(&now_time_t);

        std::ostringstream oss;
        oss << std::put_time(localTime, "%Y-%m-%d %H:%M:%S");
        oss << "." << std::setfill('0') << std::setw(6) << now_us.count();

        return oss.str();
    }

    size_t Logger::getFileSize(const std::string& filename) {
        struct stat stat_buf;
        int rc = stat(filename.c_str(), &stat_buf);
        return rc == 0 ? stat_buf.st_size : 0;
    }

    void Logger::logToFile(const std::string& message, const Result res) {
        try {
            std::string date = getCurrentDate();
            std::string filename = "sdmc:/atmosphere/contents/430000000000000B/log_" + date + ".txt";
            if (getFileSize(filename) >= m_maxLogSize) {
                std::ofstream clear(filename, std::ios::trunc);
                clear.close();
            }

            std::string errorMessage = "Error: " + std::to_string(R_DESCRIPTION(res));
            std::ofstream logFile(filename, std::ios::app);
            if (logFile.is_open()) {
                if (message == "\n##########\r\n") {
                    logFile << message << std::endl;
                } else if (res != 0) {
                    logFile << "[" << getCurrentTimestamp() << "] " << message << " " << errorMessage << std::endl;
                } else {
                    logFile << "[" << getCurrentTimestamp() << "] " << message << std::endl;
                }

                logFile.close();
            }
        } catch (...) {
            return;
        }
    }
}
