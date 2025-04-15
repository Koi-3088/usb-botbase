#pragma once

#include <string>

namespace SbbLog {
	class Logger {
	public:
		Logger() {}
		~Logger() {}

	public:
		static void logToFile(const std::string& message);

	private:
		static std::string getCurrentDate();
		static std::string getCurrentTimestamp();
	};
}
