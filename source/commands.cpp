#include "defines.h"
#include "commands.h"
#include "logger.h"
#include "util.h"
#include <switch.h>
#include "moduleBase.h"

namespace Commands {
	using namespace SbbLog;
	using namespace Util;
	using namespace ModuleBase;

	std::vector<char> CommandHandler::HandleCommand(const std::string& cmd, const std::vector<std::string>& params, int sockfd) {
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

		CommandEnum cmdEnum;
		auto it = CommandHandler::m_cmd.find(cmd);
		if (it != CommandHandler::m_cmd.end()) {
			cmdEnum = it->second;
		}
		else {
			Logger::logToFile("HandleCommand cmd not found (" + cmd + ").");
			return buffer;
		}

		switch (cmdEnum) {
		case CommandEnum::GetVersion: {
			auto sbb = getSbbVersion();
			buffer.insert(buffer.begin(), sbb.begin(), sbb.end());
			Logger::logToFile("cmd buffer: " + std::string(buffer.data()));
			return buffer;
		}
		case CommandEnum::GetTitleID: {
			MetaData meta = getMetaData();
			Logger::logToFile("got meta for title ID");

			buffer.resize(sizeof(meta.titleID));
			std::copy(reinterpret_cast<const char*>(&meta.titleID),
				reinterpret_cast<const char*>(&meta.titleID) + sizeof(meta.titleID),
				buffer.begin());

			return buffer;
		}
		case CommandEnum::Game: { // No subcommands yet - checks only version currently
			NsApplicationControlData* buf = (NsApplicationControlData*)malloc(sizeof(NsApplicationControlData));
			getoutsize(buf);
			std::string ver(buf->nacp.display_version);
			buffer.insert(buffer.begin(), ver.begin(), ver.end());
			free(buf);
			return buffer;
		}
		case CommandEnum::Configure: {
			break;
		}
		case CommandEnum::Peek: {
			if (params.size() != 2) {
				break;
			}

			MetaData meta = getMetaData();
			u64 offset = Utils::parseStringToInt(params[0]);
			Logger::logToFile("Peek parseStringToInt() offset: " + std::to_string(offset));

			u64 size = Utils::parseStringToInt(params[1]);
			Logger::logToFile("Peek parseStringToInt() size: " + std::to_string(size));

			buffer = peekInfinite(meta.heap_base + offset, size);
			Logger::logToFile("Peek buffer after peekInfinite(): " + std::string(buffer.data()));
			break;
		}
		case CommandEnum::Click: {
			if (params.size() != 1) {
				break;
			}

			click((HidNpadButton)parseStringToButton(params[0]));
			break;
		}
		case CommandEnum::ScreenOn: {
			setScreen(ViPowerState_On);
			break;
		}
		case CommandEnum::ScreenOff: {
			setScreen(ViPowerState_Off);
			break;
		}
		case CommandEnum::DetachController: {
			detachController();
			break;
		}
		case CommandEnum::PixelPeek: {
			size_t outSize = 0;
			size_t size = 0x80000;
			buffer.resize(size);

			Result rc = capsscCaptureJpegScreenShot(&outSize, buffer.data(), size, ViLayerStack_Screenshot, 1e+9L);
			if (R_FAILED(rc)) {
				Logger::logToFile("Failed to capture screenshot.");
			}

			return buffer;
		}
		}
		return buffer;
	}
}
