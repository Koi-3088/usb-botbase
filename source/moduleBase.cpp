#include "ntp.h"
#include <switch.h>
#include "logger.h"
#include "util.h"
#include "moduleBase.h"

namespace ModuleBase {
	using namespace Util;
    using namespace SbbLog;

	bool BaseCommands::attach(u64& pid) {
        Result rc = pmdmntGetApplicationProcessId(&pid);
        if (R_FAILED(rc)) {
            Logger::logToFile("attach() pmdmntGetApplicationProcessId() failed.", rc);
			return false;
        }

        if (m_debugHandle != 0) {
            svcCloseHandle(m_debugHandle);
        }

        rc = svcDebugActiveProcess(&m_debugHandle, pid);
        if (R_FAILED(rc)) {
            Logger::logToFile("attach() svcDebugActiveProcess() failed.", rc);
			return false;
        }

		return true;
    }

	void BaseCommands::detach() {
        if (m_debugHandle != 0) {
            svcCloseHandle(m_debugHandle);
        }
    }

	void BaseCommands::initMetaData() {
		u64 pid = 0;
		if (!attach(pid)) {
			Logger::logToFile("initMetaData() attach() failed.");
			detach();
			return;
		}

		if (m_metaData.pid == 0 || m_metaData.pid != pid) {
			m_metaData.pid = pid;
			m_metaData.main_nso_base = getMainNsoBase(pid);
			m_metaData.heap_base = getHeapBase();
			m_metaData.titleID = getTitleId(pid);
			m_metaData.titleVersion = GetTitleVersion(m_metaData.titleID);
			m_metaData.buildID = getBuildID(pid);
		}

		detach();
	}

	u8 BaseCommands::getBuildID(u64 pid) {
		LoaderModuleInfo proc_modules[2];
		s32 numModules = 0;
		Result rc = ldrDmntGetProcessModuleInfo(pid, proc_modules, 2, &numModules);
		if (R_FAILED(rc)) {
			Logger::logToFile("getBuildID() ldrDmntGetProcessModuleInfo() failed.", rc);
		}

		if (numModules == 2) {
			return proc_modules[1].build_id[0];
		}
		else {
			return proc_modules[0].build_id[0];
		}
	}

	u64 BaseCommands::getMainNsoBase(u64 pid) {
		LoaderModuleInfo proc_modules[2];
		s32 numModules = 0;
		Result rc = ldrDmntGetProcessModuleInfo(pid, proc_modules, 2, &numModules);
		if (R_FAILED(rc)) {
			Logger::logToFile("getMainNsoBase() ldrDmntGetProcessModuleInfo() failed.", rc);
		}

		if (numModules == 2) {
			return proc_modules[1].base_address;
		}
		else {
			return proc_modules[0].base_address;
		}
	}

	u64 BaseCommands::getHeapBase() {
		u64 heap_base = 0;
		Result rc = svcGetInfo(&heap_base, InfoType_HeapRegionAddress, m_debugHandle, 0);
		if (R_FAILED(rc)) {
			Logger::logToFile("getHeapBase() svcGetInfo() failed.", rc);
		}

		return heap_base;
	}

	u64 BaseCommands::getTitleId(u64 pid) {
		u64 titleId = 0;
		Result rc = pminfoGetProgramId(&titleId, pid);
		if (R_FAILED(rc)) {
			Logger::logToFile("getTitleId() pminfoGetProgramId() failed.", rc);
		}

		return titleId;
	}

	u64 BaseCommands::GetTitleVersion(u64 titleID) {
		Result rc = nsInitialize();
		if (R_FAILED(rc)) {
			Logger::logToFile("GetTitleVersion() nsInitialize() failed.", rc);
		}

		u64 titleV = 0;
		s32 out = 0;
		std::vector<NsApplicationContentMetaStatus> metaStatus(100U);
		rc = nsListApplicationContentMetaStatus(titleID, 0, metaStatus.data(), sizeof(NsApplicationContentMetaStatus), &out);
		nsExit();
		if (R_FAILED(rc)) {
			Logger::logToFile("GetTitleVersion() nsListApplicationContentMetaStatus() failed.", rc);
			return 0;
		}

		for (int i = 0; i < out; i++) {
			if (titleV < metaStatus[i].version) {
				titleV = metaStatus[i].version;
			}
		}

		return (titleV / 0x10000);
	}

	std::vector<NsApplicationControlData> BaseCommands::getNsApplicationControlData(u64& out) {
		Result rc = nsInitialize();
		if (R_FAILED(rc)) {
			Logger::logToFile("getNsApplicationControlData() nsInitialize() failed.", rc);
			return {};
		}

		std::vector<NsApplicationControlData> buf(1);
		rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, m_metaData.titleID, buf.data(), sizeof(NsApplicationControlData), &out);
		nsExit();
		if (R_FAILED(rc)) {
			Logger::logToFile("getNsApplicationControlData() nsGetApplicationControlData() failed.", rc);
			return {};
		}

		return buf;
	}

	void BaseCommands::setScreen(const ViPowerState& state) {
		ViDisplay temp_display;
		Result rc = viOpenDisplay("Internal", &temp_display);
		if (R_FAILED(rc)) {
			Logger::logToFile("setScreen() viOpenDisplay() failed.", rc);
			rc = viOpenDefaultDisplay(&temp_display);
		}

		if (R_SUCCEEDED(rc)) {
			rc = viSetDisplayPowerState(&temp_display, state);
			svcSleepThread(1e+6l);
			viCloseDisplay(&temp_display);

			rc = lblInitialize();
			if (R_FAILED(rc)) {
				Logger::logToFile("setScreen() lblInitialize() failed.", rc);
			}

			if (state == ViPowerState_On) {
				lblSwitchBacklightOn(1ul);
			}
			else {
				lblSwitchBacklightOff(1ul);
			}

			lblExit();
		}
	}

	bool BaseCommands::getIsProgramOpen(u64 id) {
		u64 pid = 0;
		Result rc = pmdmntGetProcessId(&pid, id);
		return !(pid == 0 || R_FAILED(rc));
	}

	void BaseCommands::setMainLoopSleepTime(const std::vector<std::string>& params) {
		mainLoopSleepTime = Utils::parseStringToInt(params[1]);
	}

	void BaseCommands::setButtonClickSleepTime(const std::vector<std::string>& params) {
		buttonClickSleepTime = Utils::parseStringToInt(params[1]);
	}

	void BaseCommands::setKeySleepTime(const std::vector<std::string>& params) {
		keyPressSleepTime = Utils::parseStringToInt(params[1]);
	}

	void BaseCommands::setFingerDiameter(const std::vector<std::string>& params) {
		fingerDiameter = Utils::parseStringToInt(params[1]);
	}

	void BaseCommands::setPollRate(const std::vector<std::string>& params) {
		pollRate = Utils::parseStringToInt(params[1]);
	}

	void BaseCommands::getGameIcon(std::vector<char>& buffer) {
		u64 out = 0;
		auto data = getNsApplicationControlData(out);
		if (data.empty()) {
			return;
		}

		out -= sizeof(data[0].nacp);
		buffer.resize(out);
		std::copy(reinterpret_cast<const char*>(&data[0].icon),
			reinterpret_cast<const char*>(&data[0].icon) + out,
			buffer.begin());
	}

	void BaseCommands::getGameVersion(std::vector<char>& buffer) {
		u64 out = 0;
		auto data = getNsApplicationControlData(out);
		if (data.empty()) {
			return;
		}

		buffer.resize(sizeof(data[0].nacp.display_version));
		std::copy(reinterpret_cast<const char*>(&data[0].nacp.display_version[0]),
			reinterpret_cast<const char*>(&data[0].nacp.display_version[15]),
			buffer.begin());
	}

	void BaseCommands::getGameRating(std::vector<char>& buffer) {
		u64 out = 0;
		auto data = getNsApplicationControlData(out);
		if (data.empty()) {
			return;
		}

		buffer.resize(sizeof(int));
		std::copy(reinterpret_cast<const char*>(&data[0].nacp.rating_age[0]),
			reinterpret_cast<const char*>(&data[0].nacp.rating_age[1] + sizeof(int)),
			buffer.begin());
	}

	void BaseCommands::getGameAuthor(std::vector<char>& buffer) {
		u64 out = 0;
		auto data = getNsApplicationControlData(out);
		if (data.empty()) {
			return;
		}

		NacpLanguageEntry* lang = nullptr;	
		Result rc = nacpGetLanguageEntry(&data[0].nacp, &lang);
		if (R_FAILED(rc)) {
			Logger::logToFile("getGameAuthor() nacpGetLanguageEntry() failed.", rc);
			delete lang;
			return;
		}

		buffer.resize(out);
		std::copy(reinterpret_cast<const char*>(&lang->author[0]),
			reinterpret_cast<const char*>(&lang->author[255]),
			buffer.begin());
		delete lang;
	}

	void BaseCommands::getGameName(std::vector<char>& buffer) {
		u64 out = 0;
		auto data = getNsApplicationControlData(out);
		if (data.empty()) {
			return;
		}

		NacpLanguageEntry* lang = nullptr;
		Result rc = nacpGetLanguageEntry(&data[0].nacp, &lang);
		if (R_FAILED(rc)) {
			Logger::logToFile("getGameName() nacpGetLanguageEntry() failed.", rc);
			delete lang;
			return;
		}

		buffer.resize(out);
		std::copy(reinterpret_cast<const char*>(&lang->name[0]),
			reinterpret_cast<const char*>(&lang->name[511]),
			buffer.begin());
		delete lang;
	}

	void BaseCommands::getSwitchTime(std::vector<char>& buffer) {
		time_t posix = 0;
		Result rc = timeGetCurrentTime(TimeType_UserSystemClock, (u64*)&posix);
		if (R_FAILED(rc)) {
			Logger::logToFile("getSwitchTime() timeGetCurrentTime(TimeType_UserSystemClock) failed.", rc);
			return;
		}

		struct tm* time = localtime(&posix);
		if (time->tm_year >= 160 || time->tm_year < 100) {
			time->tm_year = 100;
			time->tm_mon = 0;
			time->tm_mday = 1;

			rc = timeSetCurrentTime(TimeType_NetworkSystemClock, mktime(time));
			if (R_FAILED(rc)) {
				Logger::logToFile("getSwitchTime() timeGetCurrentTime(TimeType_NetworkSystemClock) failed.", rc);
				return;
			}

			posix = mktime(time);
			buffer.resize(sizeof(posix));
			free(time);

			std::copy(reinterpret_cast<const char*>(&posix),
				reinterpret_cast<const char*>(&posix) + sizeof(posix),
				buffer.begin());
		}
	}

	void BaseCommands::setSwitchTime(const std::vector<std::string>& params, std::vector<char>& buffer) {
		time_t input = (time_t)std::stoull(params[0], NULL, 10);
		struct tm* toSet = localtime(&input);
		if (toSet->tm_year >= 160 || toSet->tm_year < 100) {
			Logger::logToFile("setSwitchTime() time to set is outside of valid range.");
			return;
		}

		Result rc = timeSetCurrentTime(TimeType_NetworkSystemClock, input);
		if (R_FAILED(rc)) {
			Logger::logToFile("setSwitchTime() failed to set the network clock. Is internet time sync enabled?", rc);
			return;
		}
	}

	void BaseCommands::resetSwitchTime(std::vector<char>& buffer) {
		bool success = false;
		buffer.resize(sizeof(success));

		Result rc = setsysInitialize();
		if (R_SUCCEEDED(rc)) {
			bool sync;
			rc = setsysIsUserSystemClockAutomaticCorrectionEnabled(&sync);
			setsysExit();

			if (R_SUCCEEDED(rc) && isConnectedToInternet()) {
				time_t ntp = ntpGetTime();
				rc = timeSetCurrentTime(TimeType_NetworkSystemClock, ntp);
				if (R_SUCCEEDED(rc)) {
					success = true;
				}
				else {
					Logger::logToFile("resetSwitchTime() failed to set the network clock.", rc);
				}
			}
			else {
				Logger::logToFile("resetSwitchTime() failed to check if internet time sync is enabled.", rc);
			}
		}
		else {
			Logger::logToFile("resetSwitchTime() setsysInitialize() failed.", rc);
		}

		std::copy(reinterpret_cast<const char*>(&success),
			reinterpret_cast<const char*>(&success) + sizeof(success),
			buffer.begin());
	}

	bool BaseCommands::isConnectedToInternet() {
		Result rc = nifmInitialize(NifmServiceType_User);
		if (R_FAILED(rc)) {
			Logger::logToFile("isConnectedToInternet() nifmInitialize() failed.", rc);
			return false;
		}

		NifmInternetConnectionStatus status;
		rc = nifmGetInternetConnectionStatus(NULL, NULL, &status);
		nifmExit();
		if (R_FAILED(rc) || status != NifmInternetConnectionStatus_Connected) {
			Logger::logToFile("isConnectedToInternet() nifmGetInternetConnectionStatus() failed or not connected.", rc);
			return false;
		}

		return true;
	}
}
