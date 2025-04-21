#pragma once

#include "defines.h"
#include "moduleBase.h"
#include <vector>
#include <string>
#include <switch.h>

namespace MemoryCommands {
	class Vision : protected virtual ModuleBase::BaseCommands {
	public:
		Vision() : BaseCommands() {}
		~Vision() override {}

	protected:
		void peek(u64 offset, u64 size, std::vector<char>& buffer);
		void peekMulti(u64* offset, u64* size, u64 count);

		void poke(u64 offset, u64 size, u8* val);

		u64 followMainPointer(s64* jumps, size_t count);
		void readMem(const std::vector<char>& data, u64 offset, u64 size, u64 multi = 0);
		void writeMem(u64 offset, u64 size, u8* val);
	};
}
