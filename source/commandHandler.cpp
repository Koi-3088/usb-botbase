#include "defines.h"
#include "commandHandler.h"
#include "logger.h"
#include "util.h"
#include <switch.h>
#include "moduleBase.h"

namespace CommandHandler {
	using namespace SbbLog;
	using namespace Util;
	using namespace ModuleBase;
	using namespace MemoryCommands;

	std::vector<char> Handler::HandleCommand(const std::string& cmd, const std::vector<std::string>& params, int sockfd) {
		std::vector<char> buffer;
		if (cmd.empty()) {
			Logger::logToFile("HandleCommand cmd empty.");
			return buffer;
		}

		Logger::logToFile("HandleCommand cmd: " + cmd);
		Logger::logToFile("HandleCommand params#: " + std::to_string(params.size()));
		for (int i = 0; i < (int)params.size(); i++) {
			Logger::logToFile("HandleCommand param " + std::to_string(i) + ": " + params.at(i));
		}

		auto it = Handler::m_cmd.find(cmd);
		if (it != Handler::m_cmd.end()) {
			it->second(params, buffer);
		}
		else {
			Logger::logToFile("HandleCommand cmd not found (" + cmd + ").");
		}

		return buffer;
	}

	void Handler::peek_cmd(const std::vector<std::string>& params, std::vector<char>& buffer) {
		if (params.size() != 2) {
			return;
		}

		MetaData meta = getMetaData();
		u64 offset = Utils::parseStringToInt(params[0]);
		Logger::logToFile("Peek parseStringToInt() offset: " + std::to_string(offset));

		u64 size = Utils::parseStringToInt(params[1]);
		Logger::logToFile("Peek parseStringToInt() size: " + std::to_string(size));

		peek(meta.heap_base + offset, size, buffer);
		Logger::logToFile("Peek buffer after peek(): " + std::string(buffer.data()));
	}

	void Handler::peekMulti_cmd(const std::vector<std::string>& params, std::vector<char>& buffer) {
		return;
	}

	void Handler::getVersion_cmd(std::vector<char>& buffer) {
		auto sbb = getSbbVersion();
		buffer.insert(buffer.begin(), sbb.begin(), sbb.end());
		Logger::logToFile("cmd buffer: " + std::string(buffer.data()));
	}

	void Handler::getTitleID_cmd(std::vector<char>& buffer) {
		MetaData meta = getMetaData();
		Logger::logToFile("got meta for title ID");

		buffer.resize(sizeof(meta.titleID));
		std::copy(reinterpret_cast<const char*>(&meta.titleID),
			reinterpret_cast<const char*>(&meta.titleID) + sizeof(meta.titleID),
			buffer.begin());
	}

	void Handler::game_cmd(const std::vector<std::string>& params, std::vector<char>& buffer) {
		NsApplicationControlData* buf = (NsApplicationControlData*)malloc(sizeof(NsApplicationControlData));
		getOutSize(buf);
		std::string ver(buf->nacp.display_version);
		buffer.insert(buffer.begin(), ver.begin(), ver.end());
		free(buf);
	}

	void Handler::configure_cmd(const std::vector<std::string>& params) {
		return;
	}

	void Handler::click_cmd(const std::vector<std::string>& params) {
		if (params.size() != 1) {
			return;
		}

		click((HidNpadButton)parseStringToButton(params[0]));
	}

	void Handler::screenOn_cmd() {
		setScreen(ViPowerState_On);
	}

	void Handler::screenOff_cmd() {
		setScreen(ViPowerState_Off);
	}

	void Handler::detachController_cmd() {
		detachController();
	}

	void Handler::pixelPeek_cmd(std::vector<char>& buffer) {
		size_t outSize = 0;
		size_t size = 0x80000;
		buffer.resize(size);

		Result rc = capsscCaptureJpegScreenShot(&outSize, (void*)buffer.data(), size, ViLayerStack_Screenshot, 1e+9L);
		if (R_FAILED(rc)) {
			Logger::logToFile("Failed to capture screenshot.");
		}
	}
}
