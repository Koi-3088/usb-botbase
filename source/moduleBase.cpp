#include "logger.h"
#include "moduleBase.h"
#include "util.h"
#include "controllerCommands.h"
#include <switch.h>

namespace ModuleBase {
	using namespace Util;
	using namespace ControllerCommands;
    using namespace SbbLog;

	void BaseCommands::attach() {
        uint64_t pid = 0;
        Result rc = pmdmntGetApplicationProcessId(&pid);
        if (R_FAILED(rc)) {
            Logger::logToFile("attach() pmdmntGetApplicationProcessId() failed.");
        }

        if (m_debugHandle != 0) {
            svcCloseHandle(m_debugHandle);
        }

        rc = svcDebugActiveProcess(&m_debugHandle, pid);
        if (R_FAILED(rc)) {
            Logger::logToFile("attach() svcDebugActiveProcess() failed.");
        }
    }

	void BaseCommands::detach() {
        if (m_debugHandle != 0) {
            svcCloseHandle(m_debugHandle);
        }
    }

	BaseCommands::MetaData BaseCommands::getMetaData() {
		MetaData meta {};
		attach();
		Logger::logToFile("getMetaData() attach().");

		u64 pid = 0;
		Result rc = pmdmntGetApplicationProcessId(&pid);
		if (R_FAILED(rc)) {
			Logger::logToFile("getMetaData() pmdmntGetApplicationProcessId() failed.");
		}

		Logger::logToFile("getMetaData() pid: " + std::to_string(pid));

		meta.main_nso_base = getMainNsoBase(pid);
		Logger::logToFile("getMetaData() main_nso_base: " + std::to_string(meta.main_nso_base));

		meta.heap_base = getHeapBase();
		Logger::logToFile("getMetaData() heap_base: " + std::to_string(meta.heap_base));

		meta.titleID = getTitleId(pid);
		Logger::logToFile("getMetaData() titleID: " + std::to_string(meta.titleID));

		meta.titleVersion = GetTitleVersion(meta.titleID);
		Logger::logToFile("getMetaData() titleVersion: " + std::to_string(meta.titleVersion));

		meta.buildID = getBuildID(pid);
		Logger::logToFile("getMetaData() buildID: " + std::to_string(meta.buildID));

		detach();
		Logger::logToFile("getMetaData() detach().");
		return meta;
	}

	u8 BaseCommands::getBuildID(u64 pid) {
		LoaderModuleInfo proc_modules[2];
		s32 numModules = 0;
		Result rc = ldrDmntGetProcessModuleInfo(pid, proc_modules, 2, &numModules);
		if (R_FAILED(rc)) {
			Logger::logToFile("getBuildID() ldrDmntGetProcessModuleInfo() failed.");
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
			Logger::logToFile("getMainNsoBase() ldrDmntGetProcessModuleInfo() failed.");
		}

		LoaderModuleInfo* proc_module = 0;
		if (numModules == 2) {
			proc_module = &proc_modules[1];
		}
		else {
			proc_module = &proc_modules[0];
		}
		return proc_module->base_address;
	}

	u64 BaseCommands::getHeapBase() {
		u64 heap_base = 0;
		Result rc = svcGetInfo(&heap_base, InfoType_HeapRegionAddress, m_debugHandle, 0);
		if (R_FAILED(rc)) {
			Logger::logToFile("getHeapBase() svcGetInfo() failed.");
		}

		return heap_base;
	}

	u64 BaseCommands::getTitleId(u64 pid) {
		u64 titleId = 0;
		Result rc = pminfoGetProgramId(&titleId, pid);
		if (R_FAILED(rc)) {
			Logger::logToFile("getTitleId() pminfoGetProgramId() failed.");
		}

		return titleId;
	}

	u64 BaseCommands::GetTitleVersion(u64 titleID) {
		u64 titleV = 0;
		s32 out = 0;

		Result rc = nsInitialize();
		if (R_FAILED(rc)) {
			Logger::logToFile("GetTitleVersion() nsInitialize() failed.");
		}

		NsApplicationContentMetaStatus* metaStatus = (NsApplicationContentMetaStatus*)malloc(sizeof(NsApplicationContentMetaStatus[100U]));
		rc = nsListApplicationContentMetaStatus(titleID, 0, metaStatus, 100, &out); // This always fails. Check if sbb also fails.
		if (R_FAILED(rc)) {
			Logger::logToFile("GetTitleVersion() nsListApplicationContentMetaStatus() failed.");
		}

		Logger::logToFile("GetTitleVersion() out: " + std::to_string(out));
		for (int i = 0; i < out; i++) {
			if (titleV < metaStatus[i].version) {
				titleV = metaStatus[i].version;
			}
		}

		nsExit();
		free(metaStatus);
		metaStatus = nullptr;
		return (titleV / 0x10000);
	}

	u64 BaseCommands::getOutSize(NsApplicationControlData* buf) {
		Result rc = nsInitialize();
		if (R_FAILED(rc)) {
			Logger::logToFile("getoutsize() nsInitialize() failed.");
		}

		u64 outsize = 0;
		u64 pid = 0;
		pmdmntGetApplicationProcessId(&pid);
		rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, getTitleId(pid), buf, sizeof(NsApplicationControlData), &outsize);
		if (R_FAILED(rc)) {
			Logger::logToFile("getoutsize() nsGetApplicationControlData() failed.");
		}

		nsExit();
		return outsize;
	}

	void BaseCommands::setScreen(const ViPowerState& state) {
		ViDisplay temp_display;
		Result rc = viOpenDisplay("Internal", &temp_display);
		if (R_FAILED(rc)) {
			Logger::logToFile("setScreen() viOpenDisplay() failed.");
			rc = viOpenDefaultDisplay(&temp_display);
		}

		if (R_SUCCEEDED(rc)) {
			rc = viSetDisplayPowerState(&temp_display, state);
			svcSleepThread(1e+6l);
			viCloseDisplay(&temp_display);

			rc = lblInitialize();
			if (R_FAILED(rc)) {
				Logger::logToFile("setScreen() lblInitialize() failed.");
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

	void BaseCommands::setEchoCommands(const std::vector<std::string>& params) {
		echoCommands = Utils::parseStringToInt(params[1]) == 1;
	}

	void BaseCommands::setPrintDebugResultCodes(const std::vector<std::string>& params) {
		debugResultCodes = Utils::parseStringToInt(params[1]) == 1;
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

	void BaseCommands::setFreezeRate(const std::vector<std::string>& params) {
		freezeRate = Utils::parseStringToInt(params[1]);
	}

	void BaseCommands::getGameIcon(std::vector<char>& buffer) {
		NsApplicationControlData* buf = new(NsApplicationControlData);
		u64 outsize = getOutSize(buf);

		buffer.resize(sizeof(outsize));
		std::copy(reinterpret_cast<const char*>(&buf->icon),
			reinterpret_cast<const char*>(&buf->icon) + sizeof(outsize),
			buffer.begin());

		free(buf);
	}

	void BaseCommands::getGameVersion(std::vector<char>& buffer) {
		NsApplicationControlData* buf = new(NsApplicationControlData);
		u64 outsize = getOutSize(buf);
		std::string ver(buf->nacp.display_version);

		buffer.insert(buffer.begin(), ver.begin(), ver.end());
		free(buf);
	}

	void BaseCommands::getGameRating(std::vector<char>& buffer) {
		NsApplicationControlData* buf = new(NsApplicationControlData);
		getOutSize(buf);
		int rating = buf->nacp.rating_age[0];

		buffer.resize(sizeof(rating));
		std::copy(reinterpret_cast<const char*>(&rating),
			reinterpret_cast<const char*>(&rating) + sizeof(rating),
			buffer.begin());

		free(buf);
	}

	void BaseCommands::getGameAuthor(std::vector<char>& buffer) {
		NsApplicationControlData* buf = new(NsApplicationControlData);
		u64 outsize = getOutSize(buf);

		NacpLanguageEntry* lang = nullptr;
		nacpGetLanguageEntry(&buf->nacp, &lang);

		std::string author(lang->author);
		buffer.insert(buffer.begin(), author.begin(), author.end());
		free(buf);
		free(lang);
	}

	void BaseCommands::getGameName(std::vector<char>& buffer) {
		NsApplicationControlData* buf = new(NsApplicationControlData);
		u64 outsize = getOutSize(buf);

		NacpLanguageEntry* lang = nullptr;
		nacpGetLanguageEntry(&buf->nacp, &lang);

		std::string name(lang->name);
		buffer.insert(buffer.begin(), name.begin(), name.end());
		free(buf);
		free(lang);
	}
}
