#pragma once

#include "defines.h"
#include "moduleBase.h"
#include <vector>
#include <switch.h>

#define MAX_LINE_LENGTH 344 * 32 * 2

namespace MemoryCommands {
	class Vision : protected virtual ModuleBase::BaseCommands {
	public:
		Vision() : BaseCommands() {}
		~Vision() override {}

	protected:
		void peek(u64 offset, u64 size, std::vector<char>& buffer);
		void peekMulti(const std::vector<u64>& offsets, const std::vector<u64>& sizes, std::vector<char>& buffer);

		void poke(u64 offset, u64 size, const std::vector<char>& buffer);

		u64 followMainPointer(const s64& main, const std::vector<s64>& jumps, std::vector<char>& buffer);
		void readMem(const std::vector<char>& data, u64 offset, u64 size, u64 multi = 0);
		void writeMem(u64 offset, u64 size, const std::vector<char>& buffer);
	};
}
