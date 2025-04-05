#pragma once

#include <functional>
#include <iterator>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <switch.h>
#include <vector>

namespace Util {
	class Utils {
	public:
		Utils() {}
		~Utils() {}

		static bool flashLed();
		static bool isUSB();
		static void parseArgs(const std::vector<char>& argstr, std::function<void(const std::string&, const std::vector<std::string>&)> callback);

	private:
		static void sendPatternStatic(const HidsysNotificationLedPattern* pattern, const HidNpadIdType idType);
	};
}
