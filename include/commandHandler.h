#pragma once

#include "controllerCommands.h"
#include "memoryCommands.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#define REGISTER_CMD(name, function) \
    (m_cmd)[(name)] = [this](const std::vector<std::string>& params, std::vector<char>& buffer) { this->function(params, buffer); }
#define REGISTER_CMD_BUFFER(name, function) \
    (m_cmd)[(name)] = [this](const std::vector<std::string>&, std::vector<char>& buffer) { this->function(buffer); }
#define REGISTER_CMD_PARAMS(name, function) \
    (m_cmd)[(name)] = [this](const std::vector<std::string>& params, std::vector<char>&) { this->function(params); }
#define REGISTER_CMD_NOARGS(name, function) \
    (m_cmd)[(name)] = [this](const std::vector<std::string>&, std::vector<char>&) { this->function(); }

using CmdFunc = std::function<void(const std::vector<std::string>&, std::vector<char>&)>;

namespace CommandHandler {
	class Handler : public ControllerCommands::Controller, protected MemoryCommands::Vision {
	public:
		Handler() : Controller() {
#pragma region Register
			REGISTER_CMD("peek", peek_cmd);
			REGISTER_CMD("peekMulti", peekMulti_cmd);
			REGISTER_CMD("peekAbsolute", peekAbsolute_cmd);
			REGISTER_CMD("peekAbsoluteMulti", peekAbsoluteMulti_cmd);
			REGISTER_CMD("peekMain", peekMain_cmd);
			REGISTER_CMD("peekMainMulti", peekMainMulti_cmd);

			REGISTER_CMD_PARAMS("poke", poke_cmd);
			REGISTER_CMD_PARAMS("pokeAbsolute", pokeAbsolute_cmd);
			REGISTER_CMD_PARAMS("pokeMain", pokeMain_cmd);

			REGISTER_CMD("pointerAll", pointerAll_cmd);
			REGISTER_CMD("pointerRelative", pointerRelative_cmd);
			REGISTER_CMD("pointerPeek", pointerPeek_cmd);
			REGISTER_CMD("pointerPeekMulti", pointerPeekMulti_cmd);
			REGISTER_CMD_PARAMS("pointerPoke", pointerPoke_cmd);

			REGISTER_CMD_PARAMS("click", click_cmd);
			REGISTER_CMD_PARAMS("press", press_cmd);
			REGISTER_CMD_PARAMS("release", release_cmd);
			REGISTER_CMD_PARAMS("setStick", setStick_cmd);
			REGISTER_CMD_PARAMS("touch", touch_cmd);
			REGISTER_CMD_PARAMS("touchHold", touchHold_cmd);
			REGISTER_CMD_PARAMS("touchDraw", touchDraw_cmd);
			REGISTER_CMD_PARAMS("key", key_cmd);
			REGISTER_CMD_PARAMS("keyMod", keyMod_cmd);
			REGISTER_CMD_PARAMS("keyMulti", keyMulti_cmd);
			REGISTER_CMD_NOARGS("detachController", detachController_cmd);

			REGISTER_CMD_PARAMS("cqControllerState", cqControllerState_cmd);
			REGISTER_CMD_NOARGS("cqCancel", cqCancel_cmd);
			REGISTER_CMD_NOARGS("cqReplaceOnNext", cqReplaceOnNext_cmd);

			REGISTER_CMD_BUFFER("getBuildID", getBuildID_cmd);
			REGISTER_CMD_BUFFER("getTitleVersion", getTitleVersion_cmd);
			REGISTER_CMD_BUFFER("getSystemLanguage", getSystemLanguage_cmd);
			REGISTER_CMD("isProgramRunning", isProgramRunning_cmd);
			REGISTER_CMD_BUFFER("getMainNsoBase", getMainNsoBase_cmd);
			REGISTER_CMD_BUFFER("getHeapBase", getHeapBase_cmd);
			REGISTER_CMD_BUFFER("charge", charge_cmd);
			REGISTER_CMD_BUFFER("getVersion", getVersion_cmd);
			REGISTER_CMD_BUFFER("getTitleID", getTitleID_cmd);
			REGISTER_CMD("game", game_cmd);
			REGISTER_CMD_PARAMS("configure", configure_cmd);
			REGISTER_CMD_NOARGS("screenOn", screenOn_cmd);
			REGISTER_CMD_NOARGS("screenOff", screenOff_cmd);
			REGISTER_CMD_BUFFER("pixelPeek", pixelPeek_cmd);
			REGISTER_CMD("ping", ping_cmd);

			REGISTER_CMD_BUFFER("getSwitchTime", getSwitchTime_cmd);
			REGISTER_CMD("setSwitchTime", setSwitchTime_cmd);
			REGISTER_CMD_BUFFER("resetSwitchTime", resetSwitchTime_cmd);
#pragma endregion Command registration.
		};

		~Handler() override {}

	public:
		std::vector<char> HandleCommand(const std::string& cmd, const std::vector<std::string>& params);
		bool getIsEnabledPA();
		bool getIsRunningPA();

	private:
#pragma region Vision
		void peek_cmd(const std::vector<std::string>& params, std::vector<char>& buffer);
		void peekMulti_cmd(const std::vector<std::string>& params, std::vector<char>& buffer);
		void peekAbsolute_cmd(const std::vector<std::string>& params, std::vector<char>& buffer);
		void peekAbsoluteMulti_cmd(const std::vector<std::string>& params, std::vector<char>& buffer);
		void peekMain_cmd(const std::vector<std::string>& params, std::vector<char>& buffer);
		void peekMainMulti_cmd(const std::vector<std::string>& params, std::vector<char>& buffer);

		void poke_cmd(const std::vector<std::string>& params);
		void pokeAbsolute_cmd(const std::vector<std::string>& params);
		void pokeMain_cmd(const std::vector<std::string>& params);

		void pointerAll_cmd(const std::vector<std::string>& params, std::vector<char>& buffer);
		void pointerRelative_cmd(const std::vector<std::string>& params, std::vector<char>& buffer);
		void pointerPeek_cmd(const std::vector<std::string>& params, std::vector<char>& buffer);
		void pointerPeekMulti_cmd(const std::vector<std::string>& params, std::vector<char>& buffer);
		void pointerPoke_cmd(const std::vector<std::string>& params);
#pragma endregion Various memory read/write commands.
#pragma region Controller
		void click_cmd(const std::vector<std::string>& params);
		void press_cmd(const std::vector<std::string>& params);
		void release_cmd(const std::vector<std::string>& params);
		void setStick_cmd(const std::vector<std::string>& params);
		void touch_cmd(const std::vector<std::string>& params);
		void touchHold_cmd(const std::vector<std::string>& params);
		void touchDraw_cmd(const std::vector<std::string>& params);
		void key_cmd(const std::vector<std::string>& params);
		void keyMod_cmd(const std::vector<std::string>& params);
		void keyMulti_cmd(const std::vector<std::string>& params);

		void cqControllerState_cmd(const std::vector<std::string>& params);
		void cqCancel_cmd();
		void cqReplaceOnNext_cmd();
#pragma endregion Various controller commands.
#pragma region Base
		void getBuildID_cmd(std::vector<char>& buffer);
		void getTitleVersion_cmd(std::vector<char>& buffer);
		void getSystemLanguage_cmd(std::vector<char>& buffer);
		void isProgramRunning_cmd(const std::vector<std::string>& params, std::vector<char>& buffer);
		void getMainNsoBase_cmd(std::vector<char>& buffer);
		void getHeapBase_cmd(std::vector<char>& buffer);
		void charge_cmd(std::vector<char>& buffer);
		void getTitleID_cmd(std::vector<char>& buffer);
		void game_cmd(const std::vector<std::string>& params, std::vector<char>& buffer);
		void screenOn_cmd();
		void screenOff_cmd();
		void detachController_cmd();
		void pixelPeek_cmd(std::vector<char>& buffer);
#pragma endregion Various base libnx commands.
#pragma region Misc
		void getVersion_cmd(std::vector<char>& buffer);
		void configure_cmd(const std::vector<std::string>& params);
		void ping_cmd(const std::vector<std::string>& params, std::vector<char>& buffer);
#pragma endregion Miscellaneous commands that get/set parameters.
#pragma region Time
		void getSwitchTime_cmd(std::vector<char>& buffer);
		void setSwitchTime_cmd(const std::vector<std::string>& params, std::vector<char>& buffer);
		void resetSwitchTime_cmd(std::vector<char>& buffer);
#pragma endregion Time commands.
		std::unordered_map<std::string, CmdFunc> m_cmd;
	};
}
