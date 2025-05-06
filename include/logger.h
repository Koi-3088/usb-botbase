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
		static std::string getCurrentDate();
		static std::string getCurrentTimestamp();
	};
}
