#include "defines.h"
#include "commandHandler.h"
#include "logger.h"
#include "util.h"
#include "moduleBase.h"
#include <algorithm>
#include <cstring>
#include <switch.h>
#include <chrono>

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

		bool controllerInit = cmd == "click" && params[0] == "UNUSED";
		if (!controllerInit && m_metaData.pid == 0) {
			Logger::logToFile("HandleCommand pid is 0, calling initMetaData().");
			initMetaData();
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

	// All peek***, peek***Multi, poke***, etc. can be condensed to 1 command each using an enum, if desired.
#pragma region Vision
	void Handler::peek_cmd(const std::vector<std::string>& params, std::vector<char>& buffer) {
		if (params.size() != 2) {
			return;
		}

		u64 offset = Utils::parseStringToInt(params.front());
		u64 size = Utils::parseStringToInt(params[1]);
		peek(m_metaData.heap_base + offset, size, buffer);
	}

	void Handler::peekMulti_cmd(const std::vector<std::string>& params, std::vector<char>& buffer) {
		if (params.size() < 2) {
			return;
		}

		u64 itemCount = (params.size()) / 2;
		auto offsets = std::vector<u64>(itemCount);
		auto sizes = std::vector<u64>(itemCount);
		for (int i = 0; i < itemCount; ++i) {
			offsets[i] = m_metaData.heap_base + Utils::parseStringToInt(params[(i * 2)]);
			sizes[i] = Utils::parseStringToInt(params[(i * 2) + 1]);
		}

		peekMulti(offsets, sizes, buffer);
	}

	void Handler::peekAbsolute_cmd(const std::vector<std::string>& params, std::vector<char>& buffer) {
		if (params.size() != 2) {
			return;
		}

		u64 offset = Utils::parseStringToInt(params[0]);
		u64 size = Utils::parseStringToInt(params[1]);
		peek(offset, size, buffer);
	}

	void Handler::peekAbsoluteMulti_cmd(const std::vector<std::string>& params, std::vector<char>& buffer) {
		if (params.size() < 2) {
			return;
		}

		u64 itemCount = (params.size()) / 2;
		auto offsets = std::vector<u64>(itemCount);
		auto sizes = std::vector<u64>(itemCount);
		for (int i = 0; i < itemCount; ++i) {
			offsets[i] = Utils::parseStringToInt(params[(i * 2)]);
			sizes[i] = Utils::parseStringToInt(params[(i * 2) + 1]);
		}

		peekMulti(offsets, sizes, buffer);
	}

	void Handler::peekMain_cmd(const std::vector<std::string>& params, std::vector<char>& buffer) {
		if (params.size() != 2) {
			return;
		}

		u64 offset = Utils::parseStringToInt(params[0]);
		u64 size = Utils::parseStringToInt(params[1]);
		peek(m_metaData.main_nso_base + offset, size, buffer);
	}

	void Handler::peekMainMulti_cmd(const std::vector<std::string>& params, std::vector<char>& buffer) {
		if (params.size() < 2) {
			return;
		}

		size_t itemCount = (params.size()) / 2;
		auto offsets = std::vector<u64>(itemCount);
		auto sizes = std::vector<u64>(itemCount);
		for (int i = 0; i < itemCount; i++) {
			offsets[i] = m_metaData.main_nso_base + Utils::parseStringToInt(params[(i * 2)]);
			sizes[i] = Utils::parseStringToInt(params[(i * 2) + 1]);
		}

		peekMulti(offsets, sizes, buffer);
	}

	void Handler::poke_cmd(const std::vector<std::string>& params) {
		if (params.size() != 2) {
			return;
		}

		u64 offset = Utils::parseStringToInt(params[0]);
		std::vector<char> buffer = Utils::parseStringToByteBuffer(params[1]);
		poke(m_metaData.heap_base + offset, buffer.size(), buffer);
	}

	void Handler::pokeAbsolute_cmd(const std::vector<std::string>& params) {
		if (params.size() != 2) {
			return;
		}

		u64 offset = Utils::parseStringToInt(params[0]);
		std::vector<char> buffer = Utils::parseStringToByteBuffer(params[1]);
		poke(offset, buffer.size(), buffer);
	}

	void Handler::pokeMain_cmd(const std::vector<std::string>& params) {
		if (params.size() != 2) {
			return;
		}

		u64 offset = Utils::parseStringToInt(params[0]);
		std::vector<char> buffer = Utils::parseStringToByteBuffer(params[1]);
		poke(m_metaData.main_nso_base + offset, buffer.size(), buffer);
	}

	// pointerAll <first (main) jump> <additional jumps> <final jump in pointerexpr> 
	void Handler::pointerAll_cmd(const std::vector<std::string>& params, std::vector<char>& buffer) {
		if (params.size() < 2) {
			return;
		}

		std::vector<std::string> mod = params;
		s64 finalJump = Utils::parseStringToSignedLong(mod.back());
		mod.pop_back();

		s64 mainJump = Utils::parseStringToSignedLong(mod.front());
		mod.erase(mod.begin());

		int count = mod.size();
		std::vector<s64> jumps(count);
		for (int i = 0; i < count; i++) {
			jumps[i] = Utils::parseStringToSignedLong(mod[i]);
		}

		u64 val = followMainPointer(mainJump, jumps, buffer);
		if (val != 0) {
			val += finalJump;
			std::memcpy(buffer.data(), &val, sizeof(val));
		}
		else {
			Logger::logToFile("pointerAll_cmd() val is 0, not adding final jump.");
		}
	}

	// pointerRelative <first (main) jump> <additional jumps> <final jump in pointerexpr> 
	// returns offset relative to heap
	void Handler::pointerRelative_cmd(const std::vector<std::string>& params, std::vector<char>& buffer) {
		if (params.size() < 2) {
			return;
		}

		std::vector<std::string> mod = params;
		s64 finalJump = Utils::parseStringToSignedLong(mod.back());
		mod.pop_back();

		s64 mainJump = Utils::parseStringToSignedLong(mod.front());
		mod.erase(mod.begin());

		int count = mod.size();
		std::vector<s64> jumps(count);
		for (int i = 0; i < count; i++) {
			jumps[i] = Utils::parseStringToSignedLong(mod[i]);
		}

		u64 val = followMainPointer(mainJump, jumps, buffer);

		if (val != 0) {
			val += finalJump;
			val -= m_metaData.heap_base;
			std::memcpy(buffer.data(), &val, sizeof(val));
		}
		else {
			Logger::logToFile("pointerRelative_cmd() val is 0, not adding final jump.");
		}
	}

	// pointerPeek <amount of bytes in hex or dec> <first (main) jump> <additional jumps> <final jump in pointerexpr>
	// warning: no validation
	void Handler::pointerPeek_cmd(const std::vector<std::string>& params, std::vector<char>& buffer) {
		if (params.size() < 3) {
			return;
		}

		std::vector<std::string> mod = params;
		s64 finalJump = Utils::parseStringToSignedLong(mod.back());
		mod.pop_back();

		u64 size = Utils::parseStringToSignedLong(mod.front());
		mod.erase(mod.begin());

		s64 mainJump = Utils::parseStringToSignedLong(mod.front());
		mod.erase(mod.begin());

		int count = mod.size();
		std::vector<s64> jumps(count);
		for (int i = 0; i < count; i++) {
			jumps[i] = Utils::parseStringToSignedLong(mod[i]);
		}

		u64 val = followMainPointer(mainJump, jumps, buffer);
		val += finalJump;
		std::memcpy(buffer.data(), &val, sizeof(val));
		peek(val, size, buffer);
	}

	// pointerPeekMulti <amount of bytes in hex or dec> <first (main) jump> <additional jumps> <final jump in pointerexpr> split by asterisks (*)
	// warning: no validation
	// Untested
	void Handler::pointerPeekMulti_cmd(const std::vector<std::string>& params, std::vector<char>& buffer) {
		if (params.size() < 3) {
			return;
		}

		std::vector<u64> offsets;
		std::vector<u64> sizes;
		std::vector<std::vector<std::string>> groups;
		std::vector<std::string> currentGroup;

		for (const auto& param : params) {
			if (param == "*") {
				if (!currentGroup.empty()) {
					groups.push_back(currentGroup);
					currentGroup.clear();
				}
			}
			else {
				currentGroup.push_back(param);
			}
		}

		if (!currentGroup.empty()) {
			groups.push_back(currentGroup);
		}

		for (const auto& group : groups) {
			if (group.size() < 4) {
				continue;
			}

			std::vector<std::string> mod = group;
			s64 finalJump = Utils::parseStringToSignedLong(mod.back());
			mod.pop_back();

			s64 size = Utils::parseStringToSignedLong(mod.front());
			mod.erase(mod.begin());

			s64 mainJump = Utils::parseStringToSignedLong(mod.front());
			mod.erase(mod.begin());

			int count = mod.size();
			std::vector<s64> jumps(count);
			for (int i = 0; i < count; i++) {
				jumps[i] = Utils::parseStringToSignedLong(mod[i]);
			}

			u64 val = followMainPointer(mainJump, jumps, buffer);
			val += finalJump;

			offsets.push_back(val);
			sizes.push_back(size);
		}

		if (!offsets.empty() && !sizes.empty()) {
			peekMulti(offsets, sizes, buffer);
		}
	}

	// pointerPoke <data to be sent> <first (main) jump> <additional jumps> <final jump in pointerexpr>
	// warning: no validation
	void Handler::pointerPoke_cmd(const std::vector<std::string>& params) {
		if (params.size() < 3) {
			return;
		}

		std::vector<std::string> mod = params;
		s64 finalJump = Utils::parseStringToSignedLong(mod.back());
		mod.pop_back();

		std::vector<char> data = Utils::parseStringToByteBuffer(mod.front());
		mod.erase(mod.begin());

		s64 mainJump = Utils::parseStringToSignedLong(mod.front());
		mod.erase(mod.begin());

		int count = mod.size();
		std::vector<s64> jumps(count);
		for (int i = 0; i < count; i++) {
			jumps[i] = Utils::parseStringToSignedLong(mod[i]);
		}

		std::vector<char> buffer;
		u64 val = followMainPointer(mainJump, jumps, buffer);
		val += finalJump;
		poke(val, data.size(), data);
	}
#pragma endregion Various memory read/write commands.
#pragma region Controller
	void Handler::click_cmd(const std::vector<std::string>& params) {
		if (params.size() != 1) {
			return;
		}

		click((HidNpadButton)parseStringToButton(params.front()));
	}

	void Handler::press_cmd(const std::vector<std::string>& params) {
		if (params.size() != 1) {
			return;
		}

		HidNpadButton key = (HidNpadButton)parseStringToButton(params.front());
		press(key);
	}

	void Handler::release_cmd(const std::vector<std::string>& params) {
		if (params.size() != 1) {
			return;
		}

		HidNpadButton key = (HidNpadButton)parseStringToButton(params.front());
		release(key);
	}

	void Handler::setStick_cmd(const std::vector<std::string>& params) {
		if (params.size() != 3) {
			return;
		}

		int stick = parseStringToStick(params.front());
		if (stick == -1) {
			return;
		}

		int dxVal = std::stoull(params[1], NULL, 0);
		if (dxVal > JOYSTICK_MAX) {
			dxVal = JOYSTICK_MAX;
		}
		else if (dxVal < JOYSTICK_MIN) {
			dxVal = JOYSTICK_MIN;
		}
		

		int dyVal = std::stoull(params[2], NULL, 0);
		if (dyVal > JOYSTICK_MAX) {
			dyVal = JOYSTICK_MAX;
		}
		else if (dyVal < JOYSTICK_MIN) {
			dyVal = JOYSTICK_MIN;
		}

		setStickState((Joystick)stick, dxVal, dyVal);
	}

	//touch followed by arrayof: <x in the range 0-1280> <y in the range 0-720>. Array is sequential taps, not different fingers. Functions in its own thread, but will not allow the call again while running. tapcount * pollRate * 2
	void Handler::touch_cmd(const std::vector<std::string>& params) {
		if (params.size() < 2) {
			return;
		}

		u32 count = params.size() / 2;
		std::vector<HidTouchState> state(count);
		u32 j = 0;
		for (int i = 0; i < count; i++)
		{
			state[i].diameter_x = state[i].diameter_y = fingerDiameter;
			state[i].x = (u32)Utils::parseStringToInt(params[j++]);
			state[i].y = (u32)Utils::parseStringToInt(params[j++]);
		}

		touch(state, count, pollRate * 1e+6L, false);
	}

	//touchHold <x in the range 0-1280> <y in the range 0-720> <time in milliseconds (must be at least 15ms)>. Functions in its own thread, but will not allow the call again while running. pollRate + holdtime
	void Handler::touchHold_cmd(const std::vector<std::string>& params) {
		if (params.size() < 3) {
			return;
		}

		std::vector<HidTouchState> state(1);
		state[0].diameter_x = state[0].diameter_y = fingerDiameter;
		state[0].x = (u32)Utils::parseStringToInt(params[0]);
		state[0].y = (u32)Utils::parseStringToInt(params[1]);
		u64 time = Utils::parseStringToInt(params[2]);
		touch(state, 1, time * 1e+6L, false);
	}

	//touchDraw followed by arrayof: <x in the range 0-1280> <y in the range 0-720>. Array is vectors of where finger moves to, then removes the finger. Functions in its own thread, but will not allow the call again while running. (vectorcount * pollRate * 2) + pollRate
	void Handler::touchDraw_cmd(const std::vector<std::string>& params) {
		if (params.size() < 2) {
			return;
		}

		u32 count = params.size() / 2;
		std::vector<HidTouchState> state(count);
		u32 j = 0;
		for (int i = 0; i < count; i++)
		{
			state[i].diameter_x = state[i].diameter_y = fingerDiameter;
			state[i].x = (u32)Utils::parseStringToInt(params[j++]);
			state[i].y = (u32)Utils::parseStringToInt(params[j++]);
		}

		touch(state, count, pollRate * 1e+6L * 2, true);
	}

	//key followed by arrayof: <HidKeyboardKey> to be pressed in sequential order
	//thank you Red (hp3721) for this functionality
	void Handler::key_cmd(const std::vector<std::string>& params) {
		if (params.size() < 1) {
			return;
		}

		u64 count = params.size();
		std::vector<HiddbgKeyboardAutoPilotState> keystates(count);
		for (int i = 0; i < count; i++)
		{
			u8 key = (u8)Utils::parseStringToInt(params[i]);
			if (key >= HidKeyboardKey_A && key <= HidKeyboardKey_RightGui) {
				keystates[i].keys[key / 64] = 1UL << key;
				keystates[i].modifiers = 1024UL; //numlock
			}
		}

		key(keystates, count);
	}

	//keyMod followed by arrayof: <HidKeyboardKey> <HidKeyboardModifier>(without the bitfield shift) to be pressed in sequential order
	void Handler::keyMod_cmd(const std::vector<std::string>& params) {
		if (params.size() < 2) {
			return;
		}

		u64 count = params.size() / 2;
		std::vector<HiddbgKeyboardAutoPilotState> keystates(count);
		int j = 0;
		for (int i = 0; i < count; i++)
		{
			u8 key = (u8)Utils::parseStringToInt(params[j++]);
			if (key >= HidKeyboardKey_A && key <= HidKeyboardKey_RightGui) {
				keystates[i].keys[key / 64] = 1UL << key;
				keystates[i].modifiers = BIT((u8)Utils::parseStringToInt(params[j++]));
			}
		}

		key(keystates, count);
	}

	//keyMulti followed by arrayof: <HidKeyboardKey> to be pressed at the same time.
	void Handler::keyMulti_cmd(const std::vector<std::string>& params) {
		if (params.size() < 1) {
			return;
		}

		u64 count = params.size();
		std::vector<HiddbgKeyboardAutoPilotState> keystates(count);
		for (int i = 0; i < count; i++)
		{
			u8 key = (u8)Utils::parseStringToInt(params[i]);
			if (key >= HidKeyboardKey_A && key <= HidKeyboardKey_RightGui) {
				keystates[0].keys[key / 64] |= 1UL << key;
			}
		}

		key(keystates, count);
	}

	void Handler::detachController_cmd() {
		detachController();
	}

	void Handler::clickCC_cmd(const std::vector<std::string>& params, std::vector<char>& buffer) {
		if (params.size() < 1) {
			return;
		}

		ControllerCommand cmd {};
		try {
			cmd.parseFromHex(params[0].data());
		} catch (...) {
			Logger::logToFile("Failed to parse CC command: " + params.front());
			return;
		}

		Logger::logToFile("ControllerCommand R stick x " + std::to_string(cmd.state.right_joystick_x) + " R stick y " + std::to_string(cmd.state.right_joystick_y));

		if (!cmd.state.isNeutral()) {
			CcClick(cmd, buffer);
			Controller::m_nextStateChange += std::chrono::milliseconds(cmd.milliseconds);
		} else {
			CcClear(cmd, buffer);
			//Controller::m_nextStateChange += std::chrono::milliseconds(cmd.milliseconds);
		}
	}
#pragma endregion Various controller commands.
#pragma region Base
	void Handler::game_cmd(const std::vector<std::string>& params, std::vector<char>& buffer) {
		if (params.size() != 1) {
			return;
		}

		auto it = BaseCommands::m_game.find(params.front());
		if (it != BaseCommands::m_game.end()) {
			it->second(buffer);
		}
		else {
			Logger::logToFile("game_cmd() subcommand not found.");
		}
	}

	void Handler::getTitleID_cmd(std::vector<char>& buffer) {
		buffer.resize(sizeof(m_metaData.titleID));
		std::copy(reinterpret_cast<const char*>(&m_metaData.titleID),
			reinterpret_cast<const char*>(&m_metaData.titleID) + sizeof(m_metaData.titleID),
			buffer.begin());
	}

	void Handler::getBuildID_cmd(std::vector<char>& buffer) {
		buffer.resize(sizeof(m_metaData.buildID));
		std::copy(reinterpret_cast<const char*>(&m_metaData.buildID),
			reinterpret_cast<const char*>(&m_metaData.buildID) + sizeof(m_metaData.buildID),
			buffer.begin());
	}

	void Handler::getTitleVersion_cmd(std::vector<char>& buffer) {
		buffer.resize(sizeof(m_metaData.titleVersion));
		std::copy(reinterpret_cast<const char*>(&m_metaData.titleVersion),
			reinterpret_cast<const char*>(&m_metaData.titleVersion) + sizeof(m_metaData.titleVersion),
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

		u64 programID = Utils::parseStringToInt(params.front());
		bool isRunning = getIsProgramOpen(programID);

		buffer.resize(sizeof(isRunning));
		std::copy(reinterpret_cast<const char*>(&isRunning),
			reinterpret_cast<const char*>(&isRunning) + sizeof(isRunning),
			buffer.begin());
	}

	void Handler::pixelPeek_cmd(std::vector<char>& buffer) {
		try {
			u64 outSize = 0;
			buffer.resize(0x80000);

			Result rc = capsscCaptureJpegScreenShot(&outSize, (void*)buffer.data(), buffer.size(), ViLayerStack_Screenshot, 1e+9L);
			if (R_FAILED(rc)) {
				Logger::logToFile("Failed to capture screenshot.", rc);
			}

			buffer.resize(outSize);
		}
		catch (const std::bad_alloc& e) {
			Logger::logToFile("std::bad_alloc caught in pixelPeek_cmd(): " + std::string(e.what()));
			throw;
		}
	}

	void Handler::screenOn_cmd() {
		setScreen(ViPowerState_On);
	}

	void Handler::screenOff_cmd() {
		setScreen(ViPowerState_Off);
	}

	void Handler::getMainNsoBase_cmd(std::vector<char>& buffer) {
		buffer.resize(sizeof(m_metaData.main_nso_base));
		std::copy(reinterpret_cast<const char*>(&m_metaData.main_nso_base),
			reinterpret_cast<const char*>(&m_metaData.main_nso_base) + sizeof(m_metaData.main_nso_base),
			buffer.begin());
	}

	void Handler::getHeapBase_cmd(std::vector<char>& buffer) {
		buffer.resize(sizeof(m_metaData.heap_base));
		std::copy(reinterpret_cast<const char*>(&m_metaData.heap_base),
			reinterpret_cast<const char*>(&m_metaData.heap_base) + sizeof(m_metaData.heap_base),
			buffer.begin());
	}

	void Handler::charge_cmd(std::vector<char>& buffer) {
		Result rc = psmInitialize();
		if (R_FAILED(rc)) {
			Logger::logToFile("charge_cmd() psmInitialize() failed.");
			return;
		}

		u32 charge;
		rc = psmGetBatteryChargePercentage(&charge);
		psmExit();
		if (R_FAILED(rc)) {
			Logger::logToFile("charge_cmd() psmGetBatteryChargePercentage() failed.", rc);
			return;
		}

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
	}

	void Handler::configure_cmd(const std::vector<std::string>& params) {
		if (params.size() != 2) {
			return;
		}

		if (params.front() == "controllerType") {
			setControllerType(params);
			return;
		}

		auto it = BaseCommands::m_configure.find(params.front());
		if (it != BaseCommands::m_configure.end()) {
			it->second(params);
		}
		else {
			Logger::logToFile("configure_cmd() subfunction not found.");
		}
	}

	bool Handler::getIsEnabledPA() {
		return BaseCommands::getIsEnabledPA();
	}

	void Handler::ping_cmd(std::vector<char>& buffer) {
		std::string ack = "ACK";
		buffer.insert(buffer.begin(), ack.begin(), ack.end());
	}
#pragma endregion Miscellaneous commands that get/set parameters.
#pragma region Time
	void Handler::getSwitchTime_cmd(std::vector<char>& buffer) {
		getSwitchTime(buffer);
	}

	void Handler::setSwitchTime_cmd(const std::vector<std::string>& params, std::vector<char>& buffer) {
		if (params.size() != 1) {
			return;
		}

		setSwitchTime(params, buffer);
	}

	void Handler::resetSwitchTime_cmd(std::vector<char>& buffer) {
		resetSwitchTime(buffer);
	}
#pragma endregion Time commands.
}
