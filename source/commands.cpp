#include "defines.h"
#include "commands.h"
#include "logger.h"
#include <switch.h>
#include <cstring>

namespace Commands {
	using namespace SbbLog;

	std::vector<char> CommandHandler::HandleCommand(const std::string& cmd, const std::vector<std::string>& params, int sockfd) {
		Logger::logToFile("HandleCommand cmd: " + cmd);
		std::vector<char> buffer;
		if (cmd.empty()) {
			return buffer;
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
			buffer.insert(buffer.begin(), m_sbbVersion.begin(), m_sbbVersion.end());
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
		case CommandEnum::Game: {
			NsApplicationControlData* buf = (NsApplicationControlData*)malloc(sizeof(NsApplicationControlData));
			getoutsize(buf);
			auto ver = buf->nacp.display_version;
			buffer.assign(ver, ver + std::strlen(ver));
			return buffer;
		}
		case CommandEnum::Configure: {
			break;
		}
		}
		return buffer;
	}

	void CommandHandler::attach()
	{
		uint64_t pid = 0;
		Result rc = pmdmntGetApplicationProcessId(&pid);
		if (R_FAILED(rc))
			printf("pmdmntGetApplicationProcessId: %d\n", rc);

		if (debughandle != 0)
			svcCloseHandle(debughandle);

		rc = svcDebugActiveProcess(&debughandle, pid);
		if (R_FAILED(rc))
			printf("svcDebugActiveProcess: %d\n", rc);
	}

	void CommandHandler::detach() {
		if (debughandle != 0)
			svcCloseHandle(debughandle);
	}

	CommandHandler::MetaData CommandHandler::getMetaData() {
		MetaData meta{};
		attach();
		Logger::logToFile("getMetaData() attach().");
		u64 pid = 0;
		Result rc = pmdmntGetApplicationProcessId(&pid);
		if (R_FAILED(rc))
			printf("pmdmntGetApplicationProcessId: %d\n", rc);

		Logger::logToFile("getMetaData() pmdmntGetApplicationProcessId().");
		meta.main_nso_base = getMainNsoBase(pid);
		Logger::logToFile("getMetaData() main_nso_base.");
		meta.heap_base = getHeapBase(debughandle);
		Logger::logToFile("getMetaData() heap_base.");
		meta.titleID = getTitleId(pid);
		Logger::logToFile("getMetaData() titleID.");
		meta.titleVersion = GetTitleVersion(pid, meta.titleID);
		Logger::logToFile("getMetaData() titleVersion.");
		getBuildID(&meta, pid);
		Logger::logToFile("getMetaData() buildID.");

		detach();
		Logger::logToFile("getMetaData() detach().");
		return meta;
	}

	void CommandHandler::getBuildID(MetaData* meta, u64 pid) {
		LoaderModuleInfo proc_modules[2];
		s32 numModules = 0;
		Result rc = ldrDmntGetProcessModuleInfo(pid, proc_modules, 2, &numModules);
		if (R_FAILED(rc))
			printf("ldrDmntGetProcessModuleInfo: %d\n", rc);

		LoaderModuleInfo* proc_module = 0;
		if (numModules == 2) {
			proc_module = &proc_modules[1];
		}
		else {
			proc_module = &proc_modules[0];
		}

		memcpy(meta->buildID, proc_module->build_id, 0x20);
	}

	u64 CommandHandler::getMainNsoBase(u64 pid) {
		LoaderModuleInfo proc_modules[2];
		s32 numModules = 0;
		Result rc = ldrDmntGetProcessModuleInfo(pid, proc_modules, 2, &numModules);
		if (R_FAILED(rc))
			printf("ldrDmntGetProcessModuleInfo: %d\n", rc);

		LoaderModuleInfo* proc_module = 0;
		if (numModules == 2) {
			proc_module = &proc_modules[1];
		}
		else {
			proc_module = &proc_modules[0];
		}
		return proc_module->base_address;
	}

	u64 CommandHandler::getHeapBase(Handle handle) {
		u64 heap_base = 0;
		Result rc = svcGetInfo(&heap_base, InfoType_HeapRegionAddress, debughandle, 0);
		if (R_FAILED(rc))
			printf("svcGetInfo: %d\n", rc);

		return heap_base;
	}

	u64 CommandHandler::getTitleId(u64 pid) {
		u64 titleId = 0;
		Result rc = pminfoGetProgramId(&titleId, pid);
		if (R_FAILED(rc))
			printf("pminfoGetProgramId: %d\n", rc);
		return titleId;
	}

	u64 CommandHandler::GetTitleVersion(u64 pid, u64 titleID) {
		u64 titleV = 0;
		s32 out = 0;

		Result rc = nsInitialize();
		if (R_FAILED(rc))
			Logger::logToFile("GetTitleVersion() nsInitialize() failed.");//fatalThrow(rc);

		NsApplicationContentMetaStatus* MetaStatus{};
		rc = nsListApplicationContentMetaStatus(titleID, 0, MetaStatus, 100, &out);
		nsExit();
		if (R_FAILED(rc))
			printf("nsListApplicationContentMetaStatus: %d\n", rc);
		for (int i = 0; i < out; i++) {
			if (titleV < MetaStatus[i].version) {
				titleV = MetaStatus[i].version;
			}
		}

		free(MetaStatus);
		return (titleV / 0x10000);
	}

	u64 CommandHandler::getoutsize(NsApplicationControlData* buf) {
		Result rc = nsInitialize();
		if (R_FAILED(rc))
			Logger::logToFile("getoutsize() nsInitialize() failed.");//fatalThrow(rc);
		u64 outsize = 0;
		u64 pid = 0;
		pmdmntGetApplicationProcessId(&pid);
		rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, getTitleId(pid), buf, sizeof(NsApplicationControlData), &outsize);
		if (R_FAILED(rc)) {
			printf("nsGetApplicationControlData() failed: 0x%x\n", rc);
		}
		nsExit();
		return outsize;
	}

	void CommandHandler::peekInfinite(u64 offset, u64 size)
	{
		u64 remainder = size;
		u64 total = 0;
		std::vector<char> out;
		//u8* out[65535];

		attach();
		while (remainder > 0)
		{
			u64 size = remainder > 65535 ? 65535 : remainder;
			remainder -= size;
			readMem(out.get(), offset + total, size);

			u64 i;
			for (i = 0; i < thisBuffersize; i++)
			{
				if (usb)
					usbOut[totalFetched + i] = out[i];
				else printf("%02X", out[i]);
			}

			total += size;
		}

		detach();
		if (usb)
		{
			response.size = size;
			response.data = &usbOut[0];
			sendUsbResponse(response);
		}
		else printf("\n");
	}

	void CommandHandler::readMem(u64* out, u64 offset, u64 size)
	{
		Result rc = svcReadDebugProcessMemory(out, debughandle, offset, size);
		if (R_FAILED(rc))
			printf("svcReadDebugProcessMemory: %d\n", rc);
	}
}
