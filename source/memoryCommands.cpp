#include "memoryCommands.h"
#include "logger.h"

namespace MemoryCommands {
	using namespace SbbLog;

	void Vision::peek(u64 offset, u64 size, std::vector<char>& buffer) {
		buffer.resize(size);
		attach();
		readMem(buffer, offset, size);
		detach();
	}

	void Vision::peekMulti(u64* offset, u64* size, u64 count) {
		std::vector<char> buffer;
		u64 totalSize = 0;
		for (int i = 0; i < (int)count; i++) {
			totalSize += size[i];
		}

		buffer.resize(totalSize);
		u64 ofs = 0;
		attach();
		for (int i = 0; i < (int)count; i++)
		{
			readMem(buffer, offset[i], size[i], ofs);
			ofs += size[i];
		}

		detach();
	}

	void Vision::poke(u64 offset, u64 size, u8* val) {
		attach();
		writeMem(offset, size, val);
		detach();
	}

	u64 Vision::followMainPointer(s64* jumps, size_t count) {
		u64 offset;
		u64 size = sizeof(offset);
		std::vector<char> buffer(size);
		MetaData meta = getMetaData();

		attach();
		readMem(buffer, meta.main_nso_base + jumps[0], size);
		offset = *(u64*)buffer.data();

		for (int i = 1; i < (int)count; ++i) {
			readMem(buffer, offset + jumps[i], size);
			offset = *(u64*)buffer.data();
			if (offset == 0) {
				break;
			}
		}

		detach();
		return offset;
	}

	void Vision::readMem(const std::vector<char>& data, u64 offset, u64 size, u64 multi) {
		Result rc = svcReadDebugProcessMemory((void*)(data.data() + multi), m_debugHandle, offset, size);
		if (R_FAILED(rc)) {
			Logger::logToFile("readMem() svcReadDebugProcessMemory() failed.");
		}
		else {
			Logger::logToFile("readMem() data returned: " + std::string(data.data()));
		}
	}

	void Vision::writeMem(u64 offset, u64 size, u8* val) {
		Result rc = svcWriteDebugProcessMemory(m_debugHandle, val, offset, size);
		if (R_FAILED(rc)) {
			Logger::logToFile("writedMem() svcWriteDebugProcessMemory() failed.");
		}
	}
}
