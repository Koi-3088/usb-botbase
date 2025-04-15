#pragma once

#include "defines.h"
#include <vector>
#include <string>
#include <switch.h>

namespace ModuleBase {
	class BaseCommands {
	public:
		BaseCommands() {}
		virtual ~BaseCommands() {}

	protected:
		Handle m_debugHandle = 0;

		typedef struct {
			u64 main_nso_base;
			u64 heap_base;
			u64 titleID;
			u64 titleVersion;
			u8 buildID;
		} MetaData;

	private:
		const std::string m_sbbVersion = "2.4.1\r\n";

	protected:
		std::string getSbbVersion() {
			return m_sbbVersion;
		}

		void attach();
		void detach();

		MetaData getMetaData();
		u64 getMainNsoBase(u64 pid);
		u64 getHeapBase(Handle handle);
		u64 getTitleId(u64 pid);
		u8 getBuildID(u64 pid);
		u64 GetTitleVersion(u64 pid, u64 titleID);
		u64 getoutsize(NsApplicationControlData* buf);

		void setScreen(const ViPowerState& state);

		std::vector<char> peekInfinite(u64 offset, u64 size);
		void readMem(const std::vector<char>& data, u64 offset, u64 size);
	};
}
