#include "defines.h"
#include "logger.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <switch.h>
#include <sys\stat.h>
#include <chrono>
#include <mutex>

namespace SbbLog {
    size_t Logger::m_maxLogSize = 1024 * 1024 * 8;
    std::atomic<bool> Logger::m_isLoggingEnabled { false };

    std::string Logger::getCurrentTimestamp() {
        using namespace std::chrono;

        u64 now_sec = 0;
        Result res = timeGetCurrentTime(TimeType_UserSystemClock, &now_sec);
        if (R_FAILED(res)) {
            Logger::logToFile("Failed to get current time", std::to_string(R_DESCRIPTION(res)));
            now_sec = static_cast<u64>(std::time(nullptr));
        }

        auto now = system_clock::now();
        auto now_us = duration_cast<microseconds>(now.time_since_epoch()) % 1000000;

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

    void Logger::logToFile(const std::string& message, const std::string& error, bool override) {
        try {
            if (!m_isLoggingEnabled.load(std::memory_order_acquire) && error.empty() && !override) {
                return;
            }

            std::string filename = "sdmc:/atmosphere/contents/430000000000000B/log.txt";
            if (getFileSize(filename) >= m_maxLogSize) {
                std::ofstream clear(filename, std::ios::trunc);
                clear.close();
            }

            std::string errorMessage = "Error: " + error;
            std::ofstream logFile(filename, std::ios::app);
            if (logFile.is_open()) {
                if (!error.empty()) {
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

    void Logger::enableLogs(bool enable) {
        m_isLoggingEnabled.store(enable, std::memory_order_release);
        if (enable) {
            logToFile("Logging enabled.");
        } else {
            logToFile("Logging disabled.");
        }
    }

    bool Logger::isLoggingEnabled() {
        return m_isLoggingEnabled.load(std::memory_order_acquire);
    }
}
