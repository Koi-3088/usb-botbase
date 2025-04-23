#pragma once

#include "defines.h"
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <memory>
#include <switch.h>
#include "connection.h"
#include "controllerCommands.h"
#include "memoryCommands.h"

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
	class Handler : protected ControllerCommands::Controller, protected MemoryCommands::Vision {
	public:
		Handler() : Controller() {
			REGISTER_CMD("peek", peek_cmd);
			REGISTER_CMD("peekMulti", peekMulti_cmd);
			REGISTER_CMD_PARAMS("peekAbsolute", peekAbsolute_cmd);
			REGISTER_CMD_PARAMS("peekAbsoluteMulti", peekAbsoluteMulti_cmd);
			REGISTER_CMD_PARAMS("peekMain", peekMain_cmd);
			REGISTER_CMD_PARAMS("peekMainMulti", peekMainMulti_cmd);

			REGISTER_CMD_PARAMS("poke", poke_cmd);
			REGISTER_CMD_PARAMS("pokeAbsolute", pokeAbsolute_cmd);
			REGISTER_CMD_PARAMS("pokeMain", pokeMain_cmd);

			REGISTER_CMD_PARAMS("pointer", pointer_cmd);
			REGISTER_CMD_PARAMS("pointerAll", pointerAll_cmd);
			REGISTER_CMD_PARAMS("pointerRelative", pointerRelative_cmd);
			REGISTER_CMD_PARAMS("pointerPeek", pointerPeek_cmd);
			REGISTER_CMD_PARAMS("pointerPeekMulti", pointerPeekMulti_cmd);
			REGISTER_CMD_PARAMS("pointerPoke", pointerPoke_cmd);

			REGISTER_CMD_PARAMS("clickSeq", clickSeq_cmd);
			REGISTER_CMD_NOARGS("clickCancel", clickCancel_cmd);
			REGISTER_CMD_PARAMS("press", press_cmd);
			REGISTER_CMD_PARAMS("release", release_cmd);
			REGISTER_CMD_PARAMS("setStick", setStick_cmd);
			REGISTER_CMD_PARAMS("touch", touch_cmd);
			REGISTER_CMD_PARAMS("touchHold", touchHold_cmd);
			REGISTER_CMD_PARAMS("key", key_cmd);
			REGISTER_CMD_PARAMS("keyMod", keyMod_cmd);
			REGISTER_CMD_PARAMS("keyMulti", keyMulti_cmd);

			REGISTER_CMD_BUFFER("getBuildID", getBuildID_cmd);
			REGISTER_CMD_BUFFER("getTitleVersion", getTitleVersion_cmd);
			REGISTER_CMD_BUFFER("getSystemLanguage", getSystemLanguage_cmd);
			REGISTER_CMD_PARAMS("isProgramRunning", isProgramRunning_cmd);
			REGISTER_CMD_BUFFER("getMainNsoBase", getMainNsoBase_cmd);
			REGISTER_CMD_BUFFER("getHeapBase", getHeapBase_cmd);
			REGISTER_CMD_BUFFER("charge", charge_cmd);
			REGISTER_CMD_BUFFER("getVersion", getVersion_cmd);
			REGISTER_CMD_BUFFER("getTitleID", getTitleID_cmd);
			REGISTER_CMD("game", game_cmd);
			REGISTER_CMD_PARAMS("configure", configure_cmd);
			REGISTER_CMD_PARAMS("click", click_cmd);
			REGISTER_CMD_NOARGS("screenOn", screenOn_cmd);
			REGISTER_CMD_NOARGS("screenOff", screenOff_cmd);
			REGISTER_CMD_NOARGS("detachController", detachController_cmd);
			REGISTER_CMD_BUFFER("pixelPeek", pixelPeek_cmd);
		};

		~Handler() override {}

	public:
		std::vector<char> HandleCommand(const std::string& cmd, const std::vector<std::string>& params);

	private:
		void peek_cmd(const std::vector<std::string>& params, std::vector<char>& buffer);
		void peekMulti_cmd(const std::vector<std::string>& params, std::vector<char>& buffer);
		void peekAbsolute_cmd(const std::vector<std::string>& params);
		void peekAbsoluteMulti_cmd(const std::vector<std::string>& params);
		void peekMain_cmd(const std::vector<std::string>& params);
		void peekMainMulti_cmd(const std::vector<std::string>& params);

		void poke_cmd(const std::vector<std::string>& params);
		void pokeAbsolute_cmd(const std::vector<std::string>& params);
		void pokeMain_cmd(const std::vector<std::string>& params);

		void pointer_cmd(const std::vector<std::string>& params);
		void pointerAll_cmd(const std::vector<std::string>& params);
		void pointerRelative_cmd(const std::vector<std::string>& params);
		void pointerPeek_cmd(const std::vector<std::string>& params);
		void pointerPeekMulti_cmd(const std::vector<std::string>& params);
		void pointerPoke_cmd(const std::vector<std::string>& params);

		void clickSeq_cmd(const std::vector<std::string>& params);
		void clickCancel_cmd();
		void press_cmd(const std::vector<std::string>& params);
		void release_cmd(const std::vector<std::string>& params);
		void setStick_cmd(const std::vector<std::string>& params);
		void touch_cmd(const std::vector<std::string>& params);
		void touchHold_cmd(const std::vector<std::string>& params);
		void key_cmd(const std::vector<std::string>& params);
		void keyMod_cmd(const std::vector<std::string>& params);
		void keyMulti_cmd(const std::vector<std::string>& params);

		void getBuildID_cmd(std::vector<char>& buffer);
		void getTitleVersion_cmd(std::vector<char>& buffer);
		void getSystemLanguage_cmd(std::vector<char>& buffer);
		void isProgramRunning_cmd(const std::vector<std::string>& params);
		void getMainNsoBase_cmd(std::vector<char>& buffer);
		void getHeapBase_cmd(std::vector<char>& buffer);
		void charge_cmd(std::vector<char>& buffer);
		void getVersion_cmd(std::vector<char>& buffer);
		void getTitleID_cmd(std::vector<char>& buffer);
		void game_cmd(const std::vector<std::string>& params, std::vector<char>& buffer);
		void configure_cmd(const std::vector<std::string>& params);
		void click_cmd(const std::vector<std::string>& params);
		void screenOn_cmd();
		void screenOff_cmd();
		void detachController_cmd();
		void pixelPeek_cmd(std::vector<char>& buffer);

		std::unordered_map<std::string, CmdFunc> m_cmd;
	};
}
