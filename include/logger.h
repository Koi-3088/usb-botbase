#pragma once

#include <string>
#include <switch.h>
#include <atomic>

namespace SbbLog {
	class Logger {
	public:
		Logger() {}
		~Logger() {}

	private:
		static size_t m_maxLogSize;
		static std::string getCurrentTimestamp();
		static size_t getFileSize(const std::string& filename);
		static std::atomic<bool> m_isLoggingEnabled;

	public:
		static void logToFile(const std::string& message, const std::string& error = "", bool override = false);
		static void enableLogs(bool enable);
		static bool isLoggingEnabled();
	};
}
