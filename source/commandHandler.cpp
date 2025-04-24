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

	std::vector<char> Handler::HandleCommand(const std::string& cmd, const std::vector<std::string>& params) {
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

	// All peek***, peek***multi, poke***, etc. can be condensed to 1 command each using an enum
#pragma region Vision
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
		if (params.size() < 2 || params.size() % 2 == 0) {
			return;
		}
	}

	void Handler::peekAbsolute_cmd(const std::vector<std::string>& params) {
		if (params.size() != 2) {
			return;
		}
	}

	void Handler::peekAbsoluteMulti_cmd(const std::vector<std::string>& params) {
		if (params.size() < 2 || params.size() % 2 == 0) {
			return;
		}
	}

	void Handler::peekMain_cmd(const std::vector<std::string>& params) {
		if (params.size() != 2) {
			return;
		}
	}

	void Handler::peekMainMulti_cmd(const std::vector<std::string>& params) {
		if (params.size() < 2 || params.size() % 2 == 0) {
			return;
		}
	}

	void Handler::poke_cmd(const std::vector<std::string>& params) {
		if (params.size() != 2) {
			return;
		}
	}

	void Handler::pokeAbsolute_cmd(const std::vector<std::string>& params) {
		if (params.size() != 2) {
			return;
		}
	}

	void Handler::pokeMain_cmd(const std::vector<std::string>& params) {
		if (params.size() != 2) {
			return;
		}
	}

	void Handler::pointer_cmd(const std::vector<std::string>& params) {
		if (params.size() != 1) {
			return;
		}
	}

	void Handler::pointerAll_cmd(const std::vector<std::string>& params) {
		if (params.size() < 2) {
			return;
		}
	}

	void Handler::pointerRelative_cmd(const std::vector<std::string>& params) {
		if (params.size() < 2) {
			return;
		}
	}

	void Handler::pointerPeek_cmd(const std::vector<std::string>& params) {
		if (params.size() < 3) {
			return;
		}
	}

	void Handler::pointerPeekMulti_cmd(const std::vector<std::string>& params) {
		if (params.size() < 3) {
			return;
		}
	}

	void Handler::pointerPoke_cmd(const std::vector<std::string>& params) {
		if (params.size() < 3) {
			return;
		}
	}
#pragma endregion Various memory read/write commands.
#pragma region Controller
	void Handler::click_cmd(const std::vector<std::string>& params) {
		if (params.size() != 1) {
			return;
		}

		click((HidNpadButton)parseStringToButton(params[0]));
	}

	void Handler::clickSeq_cmd(const std::vector<std::string>& params) {
		if (params.size() != 1) {
			return;
		}
	}

	void Handler::clickCancel_cmd() {
		return;
	}

	void Handler::press_cmd(const std::vector<std::string>& params) {
		if (params.size() != 1) {
			return;
		}
	}

	void Handler::release_cmd(const std::vector<std::string>& params) {
		if (params.size() != 1) {
			return;
		}
	}

	void Handler::setStick_cmd(const std::vector<std::string>& params) {
		if (params.size() != 3) {
			return;
		}
	}

	void Handler::touch_cmd(const std::vector<std::string>& params) {
		if (params.size() < 2 || params.size() % 2 == 0) {
			return;
		}
	}

	void Handler::touchHold_cmd(const std::vector<std::string>& params) {
		if (params.size() < 3) {
			return;
		}
	}

	void Handler::key_cmd(const std::vector<std::string>& params) {
		if (params.size() != 1) {
			return;
		}
	}

	void Handler::keyMod_cmd(const std::vector<std::string>& params) {
		if (params.size() < 2 || params.size() % 2 == 0) {
			return;
		}
	}

	void Handler::keyMulti_cmd(const std::vector<std::string>& params) {
		if (params.size() < 2) {
			return;
		}
	}

	void Handler::detachController_cmd() {
		detachController();
	}
#pragma endregion Various controller commands.
#pragma region Base
	void Handler::game_cmd(const std::vector<std::string>& params, std::vector<char>& buffer) {
		if (params.size() != 1) {
			return;
		}

		auto it = BaseCommands::m_game.find(params[0]);
		if (it != BaseCommands::m_game.end()) {
			it->second(buffer);
		}
		else {
			Logger::logToFile("game_cmd() subcommand not found.");
			return;
		}
	}

	void Handler::getTitleID_cmd(std::vector<char>& buffer) {
		MetaData meta = getMetaData();
		Logger::logToFile("got meta for title ID");

		buffer.resize(sizeof(meta.titleID));
		std::copy(reinterpret_cast<const char*>(&meta.titleID),
			reinterpret_cast<const char*>(&meta.titleID) + sizeof(meta.titleID),
			buffer.begin());
	}

	void Handler::getBuildID_cmd(std::vector<char>& buffer) {
		MetaData meta = getMetaData();
		Logger::logToFile("got meta for title ID");

		buffer.resize(sizeof(meta.buildID));
		std::copy(reinterpret_cast<const char*>(&meta.buildID),
			reinterpret_cast<const char*>(&meta.buildID) + sizeof(meta.buildID),
			buffer.begin());
	}

	void Handler::getTitleVersion_cmd(std::vector<char>& buffer) {
		MetaData meta = getMetaData();
		Logger::logToFile("got meta for title ID");

		buffer.resize(sizeof(meta.titleVersion));
		std::copy(reinterpret_cast<const char*>(&meta.titleVersion),
			reinterpret_cast<const char*>(&meta.titleVersion) + sizeof(meta.titleVersion),
			buffer.begin());
	}

	void Handler::getSystemLanguage_cmd(std::vector<char>& buffer) {
		setInitialize();
		u64 languageCode = 0;
		SetLanguage language = SetLanguage_ENUS;
		setGetSystemLanguage(&languageCode);
		setMakeLanguage(languageCode, &language);
		setExit();

		buffer.resize(sizeof(language));
		std::copy(reinterpret_cast<const char*>(&language),
			reinterpret_cast<const char*>(&language) + sizeof(language),
			buffer.begin());
	}

	void Handler::isProgramRunning_cmd(const std::vector<std::string>& params, std::vector<char>& buffer) {
		if (params.size() != 1) {
			return;
		}

		u64 programID = Utils::parseStringToInt(params[0]);
		bool isRunning = getIsProgramOpen(programID);

		buffer.resize(sizeof(isRunning));
		std::copy(reinterpret_cast<const char*>(&isRunning),
			reinterpret_cast<const char*>(&isRunning) + sizeof(isRunning),
			buffer.begin());
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

	void Handler::screenOn_cmd() {
		setScreen(ViPowerState_On);
	}

	void Handler::screenOff_cmd() {
		setScreen(ViPowerState_Off);
	}

	void Handler::getMainNsoBase_cmd(std::vector<char>& buffer) {
		MetaData meta = getMetaData();
		buffer.resize(sizeof(meta.main_nso_base));
		std::copy(reinterpret_cast<const char*>(&meta.main_nso_base),
			reinterpret_cast<const char*>(&meta.main_nso_base) + sizeof(meta.main_nso_base),
			buffer.begin());
	}

	void Handler::getHeapBase_cmd(std::vector<char>& buffer) {
		MetaData meta = getMetaData();
		buffer.resize(sizeof(meta.heap_base));
		std::copy(reinterpret_cast<const char*>(&meta.heap_base),
			reinterpret_cast<const char*>(&meta.heap_base) + sizeof(meta.heap_base),
			buffer.begin());
	}

	void Handler::charge_cmd(std::vector<char>& buffer) {
		Result rc = psmInitialize();
		if (R_FAILED(rc)) {
			Logger::logToFile("charge_cmd() psmInitialize() failed.");
			return;
		}

		u32 charge;
		psmGetBatteryChargePercentage(&charge);
		psmExit();

		buffer.resize(sizeof(charge));
		std::copy(reinterpret_cast<const char*>(&charge),
			reinterpret_cast<const char*>(&charge) + sizeof(charge),
			buffer.begin());
	}
#pragma endregion Various base libnx commands.
#pragma region Misc
	void Handler::getVersion_cmd(std::vector<char>& buffer) {
		auto sbb = getSbbVersion();
		buffer.insert(buffer.begin(), sbb.begin(), sbb.end());
		Logger::logToFile("cmd buffer: " + std::string(buffer.data()));
	}

	void Handler::configure_cmd(const std::vector<std::string>& params) {
		if (params.size() != 2) {
			return;
		}

		if (params[0] == "controllerType") {
			setControllerType(params);
			return;
		}

		auto it = BaseCommands::m_configure.find(params[0]);
		if (it != BaseCommands::m_configure.end()) {
			it->second(params);
		}
		else {
			Logger::logToFile("configure_cmd() subfunction not found.");
		}
	}
#pragma endregion Miscellaneous commands that get/set parameters.
}
