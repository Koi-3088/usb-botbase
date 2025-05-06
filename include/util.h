#pragma once

#include <switch.h>
#include <vector>
#include <string>
#include <functional>

namespace Util {
	class Utils {
	public:
		Utils() {}
		~Utils() {}

	public:
		static bool flashLed();
		static bool isUSB();
		static void parseArgs(const std::string& cmd, std::function<void(const std::string&, const std::vector<std::string>&)> callback);
		static u64 parseStringToInt(const std::string& arg);
		static s64 parseStringToSignedLong(const std::string& arg);
		static std::vector<char> parseStringToByteBuffer(const std::string& arg);

	private:
		static void sendPatternStatic(const HidsysNotificationLedPattern* pattern, const HidNpadIdType idType);
	};
}
