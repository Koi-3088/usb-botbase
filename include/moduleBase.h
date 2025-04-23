#pragma once

#include "defines.h"
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <switch.h>

#define REGISTER_CFG_CMD(name, function) \
    (m_configure)[(name)] = [this](const std::vector<std::string>& params) { this->function(params); }

namespace ModuleBase {
	class BaseCommands {
	public:
		BaseCommands() {
			REGISTER_CFG_CMD("mainLoopSleepTime", setMainLoopSleepTime);
			REGISTER_CFG_CMD("buttonClickSleepTime", setButtonClickSleepTime);
			REGISTER_CFG_CMD("echoCommands", setEchoCommands);
			REGISTER_CFG_CMD("printDebugResultCodes", setPrintDebugResultCodes);
			REGISTER_CFG_CMD("keySleepTime", setKeySleepTime);
			REGISTER_CFG_CMD("fingerDiameter", setFingerDiameter);
			REGISTER_CFG_CMD("pollRate", setPollRate);
			REGISTER_CFG_CMD("freezeRate", setFreezeRate);
			REGISTER_CFG_CMD("controllerType", setControllerType);
		};

		virtual ~BaseCommands() {}

	protected:
		Handle m_debugHandle = 0;
		u64 mainLoopSleepTime = 50;
		u64 freezeRate = 3;
		u64 buttonClickSleepTime = 50;
		u64 keyPressSleepTime = 25;
		u64 pollRate = 17;
		u32 fingerDiameter = 50;

		bool debugResultCodes = false;
		bool echoCommands = false;

		typedef struct {
			u64 main_nso_base;
			u64 heap_base;
			u64 titleID;
			u64 titleVersion;
			u8 buildID;
		} MetaData;

		enum class Joystick {
			Left = 0,
			Right = 1,
		};

	private:
		const std::string m_sbbVersion = "2.4.1\r\n";
		std::unordered_map<std::string, std::function<void(const std::vector<std::string>&)>> m_configure;

		void setMainLoopSleepTime(const std::vector<std::string>& params);
		void setButtonClickSleepTime(const std::vector<std::string>& params);
		void setEchoCommands(const std::vector<std::string>& params);
		void setPrintDebugResultCodes(const std::vector<std::string>& params);
		void setKeySleepTime(const std::vector<std::string>& params);
		void setFingerDiameter(const std::vector<std::string>& params);
		void setPollRate(const std::vector<std::string>& params);
		void setFreezeRate(const std::vector<std::string>& params);
		void setControllerType(const std::vector<std::string>& params);

	protected:
		std::string getSbbVersion() {
			return m_sbbVersion;
		}

		void attach();
		void detach();

		MetaData getMetaData();
		u64 getMainNsoBase(u64 pid);
		u64 getHeapBase();
		u64 getTitleId(u64 pid);
		u8 getBuildID(u64 pid);
		u64 GetTitleVersion(u64 titleID);
		u64 getOutSize(NsApplicationControlData* buf);

		bool getIsProgramOpen(u64 id);
		void setScreen(const ViPowerState& state);
	};
}
