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
		std::vector<char> HandleCommand(const std::string& cmd, const std::vector<std::string>& params, int sockfd = 0);

	private:
		void peek_cmd(const std::vector<std::string>& params, std::vector<char>& buffer);
		void peekMulti_cmd(const std::vector<std::string>& params, std::vector<char>& buffer);
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
