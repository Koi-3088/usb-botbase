#pragma once

#include <stdio.h>
#include <iostream> 
#include <sstream>
#include <string.h>
#include <algorithm>
#include <iterator>
#include <vector>
#include <functional>
#include <switch.h>

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
