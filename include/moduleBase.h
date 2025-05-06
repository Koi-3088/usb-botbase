#pragma once

#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <switch.h>

#define REGISTER_CFG_CMD(name, function) \
    (m_configure)[(name)] = [this](const std::vector<std::string>& params) { this->function(params); }

#define REGISTER_GAME_CMD(name, function) \
    (m_game)[(name)] = [this](std::vector<char>& buffer) { this->function(buffer); }

namespace ModuleBase {
	class BaseCommands {
	public:
		BaseCommands() {
			REGISTER_CFG_CMD("mainLoopSleepTime", setMainLoopSleepTime);
			REGISTER_CFG_CMD("buttonClickSleepTime", setButtonClickSleepTime);
			REGISTER_CFG_CMD("keySleepTime", setKeySleepTime);
			REGISTER_CFG_CMD("fingerDiameter", setFingerDiameter);
			REGISTER_CFG_CMD("pollRate", setPollRate);

			REGISTER_GAME_CMD("icon", getGameIcon);
			REGISTER_GAME_CMD("version", getGameVersion);
			REGISTER_GAME_CMD("rating", getGameRating);
			REGISTER_GAME_CMD("author", getGameAuthor);
			REGISTER_GAME_CMD("name", getGameName);
		};

		virtual ~BaseCommands() {}

	protected:
		Handle m_debugHandle = 0;
		u64 mainLoopSleepTime = 50;
		u64 buttonClickSleepTime = 50;
		u64 keyPressSleepTime = 25;
		u64 pollRate = 17;
		u32 fingerDiameter = 50;

		struct MetaData {
			u64 main_nso_base;
			u64 heap_base;
			u64 titleID;
			u64 titleVersion;
			u64 pid;
			u8 buildID;
		};

		MetaData m_metaData { 0 };

		enum Joystick {
			Left = 0,
			Right = 1,
		};

		std::unordered_map<std::string, std::function<void(const std::vector<std::string>&)>> m_configure;
		std::unordered_map<std::string, std::function<void(std::vector<char>&)>> m_game;

	protected:
		std::string getSbbVersion() {
			return m_sbbVersion;
		}

		void attach();
		void detach();
		void initMetaData();

		u64 getMainNsoBase(u64 pid);
		u64 getHeapBase();
		u64 getTitleId(u64 pid);
		u8 getBuildID(u64 pid);
		u64 GetTitleVersion(u64 titleID);
		u64 getOutSize(NsApplicationControlData* buf);

		bool getIsProgramOpen(u64 id);
		void setScreen(const ViPowerState& state);

		void getSwitchTime(std::vector<char>& buffer);
		void setSwitchTime(const std::vector<std::string>& params, std::vector<char>& buffer);
		void resetSwitchTime(std::vector<char>& buffer);

	private:
		const std::string m_sbbVersion = "2.4.1";

		void setMainLoopSleepTime(const std::vector<std::string>& params);
		void setButtonClickSleepTime(const std::vector<std::string>& params);
		void setKeySleepTime(const std::vector<std::string>& params);
		void setFingerDiameter(const std::vector<std::string>& params);
		void setPollRate(const std::vector<std::string>& params);

		void getGameIcon(std::vector<char>& buffer);
		void getGameVersion(std::vector<char>& buffer);
		void getGameRating(std::vector<char>& buffer);
		void getGameAuthor(std::vector<char>& buffer);
		void getGameName(std::vector<char>& buffer);

		bool isConnectedToInternet();
	};
}
