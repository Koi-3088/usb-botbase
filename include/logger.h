#pragma once

#include <string>
#include <switch.h>

namespace SbbLog {
	class Logger {
	public:
		Logger() {}
		~Logger() {}

	public:
		static void logToFile(const std::string& message, const Result res = 0);

	private:
		static size_t m_maxLogSize;
		static std::string getCurrentDate();
		static std::string getCurrentTimestamp();
		static size_t getFileSize(const std::string& filename);
	};
}
