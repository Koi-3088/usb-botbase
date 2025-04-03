#pragma once

#include <string>

namespace SbbLog {
	class Logger {
	public:
		static void logToFile(const std::string& message, const std::string& filename = "sdmc:/atmosphere/contents/430000000000000B/log.txt");
	};
}
