#include "defines.h"
#include "memoryCommands.h"
#include "logger.h"
#include <cstring>

namespace MemoryCommands {
	using namespace SbbLog;

	void Vision::peek(u64 offset, u64 size, std::vector<char>& buffer) {
		u64 total = 0;
		u64 remainder = size;
		buffer.resize(size);

		while (remainder > 0) {
			u64 receive = remainder > MAX_LINE_LENGTH ? MAX_LINE_LENGTH : remainder;
			remainder -= receive;
			readMem(buffer, offset + total, receive);
			total += receive;
		}
	}

	void Vision::peekMulti(u64* offset, u64* size, u64 count, std::vector<char>& buffer) {
		u64 totalSize = 0;
		for (int i = 0; i < (int)count; i++) {
			totalSize += size[i];
		}

		buffer.resize(totalSize);
		u64 ofs = 0;
		for (int i = 0; i < (int)count; i++) {
			readMem(buffer, offset[i], size[i], ofs);
			ofs += size[i];
		}
	}

	void Vision::poke(u64 offset, u64 size, const std::vector<char>& buffer) {
		writeMem(offset, size, buffer);
	}

	u64 Vision::followMainPointer(const s64& main, const std::vector<s64>& jumps, std::vector<char>& buffer) {
		u64 offset = 0;
		u64 size = sizeof(u64);
		buffer.resize(size);

		readMem(buffer, m_metaData.main_nso_base + main, size);
		std::memcpy(&offset, buffer.data(), size);

		for (int i = 0; i < jumps.size(); i++) {
			readMem(buffer, offset + jumps[i], size);
			std::memcpy(&offset, buffer.data(), size);
			if (offset == 0) {
				break;
			}
		}

		return offset;
	}

	void Vision::readMem(const std::vector<char>& buffer, u64 offset, u64 size, u64 multi) {
		attach(m_metaData.pid);
		Result rc = svcReadDebugProcessMemory((void*)(buffer.data()), m_debugHandle, offset, size);
		if (R_FAILED(rc)) {
			Logger::logToFile("readMem() svcReadDebugProcessMemory() failed.", rc);
		}
		detach();
	}

	void Vision::writeMem(u64 offset, u64 size, const std::vector<char>& buffer) {
		attach(m_metaData.pid);
		Result rc = svcWriteDebugProcessMemory(m_debugHandle, (void*)buffer.data(), offset, size);
		if (R_FAILED(rc)) {
			Logger::logToFile("writedMem() svcWriteDebugProcessMemory() failed.", rc);
		}
		detach();
	}
}
