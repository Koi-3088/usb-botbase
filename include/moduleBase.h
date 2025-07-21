#pragma once

#include "util.h"
#include <unordered_map>
#include <atomic>
#include <switch.h>

#define REGISTER_CFG_CMD(name, function) \
    (m_configure)[(name)] = [this](const std::vector<std::string>& params) { this->function(params); }

#define REGISTER_GAME_CMD(name, function) \
    (m_game)[(name)] = [this](std::vector<char>& buffer) { this->function(buffer); }

namespace ModuleBase {
	using namespace Util;

	class BaseCommands {
	public:
		BaseCommands() {
			REGISTER_CFG_CMD("buttonClickSleepTime", setButtonClickSleepTime);
			REGISTER_CFG_CMD("keySleepTime", setKeySleepTime);
			REGISTER_CFG_CMD("fingerDiameter", setFingerDiameter);
			REGISTER_CFG_CMD("pollRate", setPollRate);
			REGISTER_CFG_CMD("enablePA", setEnabledPA);
			REGISTER_CFG_CMD("enableLogs", setEnabledLogs);
            REGISTER_CFG_CMD("enableBackwardsCompat", setEnabledBackwards);

			REGISTER_GAME_CMD("icon", getGameIcon);
			REGISTER_GAME_CMD("version", getGameVersion);
			REGISTER_GAME_CMD("rating", getGameRating);
			REGISTER_GAME_CMD("author", getGameAuthor);
			REGISTER_GAME_CMD("name", getGameName);
		};

		virtual ~BaseCommands() {}

	protected:
		Handle m_debugHandle = 0;
		u64 m_buttonClickSleepTime = 50;
		u64 m_keyPressSleepTime = 25;
		u64 m_pollRate = 17;
		u32 m_fingerDiameter = 50;
		std::atomic_bool m_isEnabledPA { false };

		struct MetaData {
			u64 main_nso_base = 0;
			u64 heap_base = 0;
			u64 titleID = 0;
			u64 titleVersion = 0;
			u64 pid = 0;
			u8 buildID = 0;
		};

		MetaData m_metaData = { 0 };
		enum Joystick {
			Left = 0,
			Right = 1,
		};

		std::unordered_map<std::string, std::function<void(const std::vector<std::string>&)>> m_configure;
		std::unordered_map<std::string, std::function<void(std::vector<char>&)>> m_game;

	protected:
		std::string getSbbVersion() {
			return getCurrentSbbVersion();
		}

		bool getIsEnabledPA() {
			return m_isEnabledPA;
		}

		bool attach();
		void detach();
		void initMetaData();

		u64 getMainNsoBase();
		u64 getHeapBase();
		u64 getTitleId();
		u8 getBuildID();
		u64 GetTitleVersion();
		std::vector<NsApplicationControlData> getNsApplicationControlData(u64& out);

		bool getIsProgramOpen(u64 id);
		void setScreen(const ViPowerState& state);

		void getSwitchTime(std::vector<char>& buffer);
		void setSwitchTime(const std::vector<std::string>& params, std::vector<char>& buffer);
		void resetSwitchTime(std::vector<char>& buffer);

	private:
		static std::string getCurrentSbbVersion() {
            return !g_enableBackwardsCompat ? "3.0\r\n" : "3.1\r\n";
        }

        void setButtonClickSleepTime(const std::vector<std::string>& params);
		void setKeySleepTime(const std::vector<std::string>& params);
		void setFingerDiameter(const std::vector<std::string>& params);
		void setPollRate(const std::vector<std::string>& params);

		void getGameIcon(std::vector<char>& buffer);
		void getGameVersion(std::vector<char>& buffer);
		void getGameRating(std::vector<char>& buffer);
		void getGameAuthor(std::vector<char>& buffer);
		void getGameName(std::vector<char>& buffer);

		void setEnabledPA(const std::vector<std::string>& params);
        void setEnabledLogs(const std::vector<std::string>& params);
        void setEnabledBackwards(const std::vector<std::string>& params);

		bool isConnectedToInternet();
		bool metaHasZeroValue(const MetaData& meta);
	};
}
