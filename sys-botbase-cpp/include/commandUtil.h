#pragma once

#include <switch.h>

namespace CommandUtil {
	class CommandUtils {
	public:
		CommandUtils() {}
		~CommandUtils() {}
		bool flashLed();

	private:
		static void sendPatternStatic(const HidsysNotificationLedPattern* pattern, const HidNpadIdType idType);
		//static const HidsysNotificationLedPattern breathingpattern;
		//static const HidsysNotificationLedPattern flashpattern;
	};
}
