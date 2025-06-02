#include "defines.h"
#include "memoryCommands.h"
#include "logger.h"
#include <cstring>

namespace MemoryCommands {
	using namespace SbbLog;

    /**
     * @brief Read memory from the specified offset and size into the buffer.
     * @param offset The memory offset.
     * @param size The number of bytes to read.
     * @param[out] buffer Output buffer for the read data.
     */
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

    /**
     * @brief Read multiple memory regions into the buffer.
     * @param offsets Vector of memory offsets.
     * @param sizes Vector of sizes for each region.
     * @param[out] buffer Output buffer for the read data.
     */
    void Vision::peekMulti(const std::vector<u64>& offsets, const std::vector<u64>& sizes, std::vector<char>& buffer) {
        u64 ofs = 0;
        u64 totalSize = 0;
        int size = (int)sizes.size();
        for (int i = 0; i < size; i++) {
            totalSize += sizes[i];
        }

        buffer.resize(totalSize * sizeof(u8));
        int count = (int)offsets.size();
        for (int i = 0; i < count; i++) {
            readMem(buffer, offsets[i], sizes[i], ofs);
            ofs += sizes[i];
        }
    }

    /**
     * @brief Write memory to the specified offset.
     * @param offset The memory offset.
     * @param size The number of bytes to write.
     * @param buffer Input buffer containing data to write.
     */
    void Vision::poke(u64 offset, u64 size, const std::vector<char>& buffer) {
        writeMem(offset, size, buffer);
    }

    /**
     * @brief Follow a pointer chain starting from main, applying jumps, and return the final address.
     * @param main The base pointer offset.
     * @param jumps Vector of jumps to follow.
     * @param[out] buffer Buffer used for intermediate reads.
     * @return The final address after following the pointer chain.
     */
    u64 Vision::followMainPointer(const s64& main, const std::vector<s64>& jumps, std::vector<char>& buffer) {
        u64 offset = 0;
        u64 size = sizeof(u64);
        buffer.resize(size);

        readMem(buffer, m_metaData.main_nso_base + main, size);
        std::memcpy(&offset, buffer.data(), size);

        int count = (int)jumps.size();
        for (int i = 0; i < count; i++) {
            readMem(buffer, offset + jumps[i], size);
            std::memcpy(&offset, buffer.data(), size);
            if (offset == 0) {
                break;
            }
        }

        return offset;
    }

    /**
     * @brief Read memory from the debugged process.
     * @param buffer Buffer to store the read data.
     * @param offset The memory offset.
     * @param size The number of bytes to read.
     * @param multi Offset into the buffer for multi-read (default 0).
     */
    void Vision::readMem(const std::vector<char>& buffer, u64 offset, u64 size, u64 multi) {
        attach();
        Result rc = svcReadDebugProcessMemory((void*)(buffer.data() + multi), m_debugHandle, offset, size);
        if (R_FAILED(rc)) {
            Logger::logToFile("readMem() svcReadDebugProcessMemory() failed.", rc);
        }
        detach();
    }

    /**
     * @brief Write memory to the debugged process.
     * @param offset The memory offset.
     * @param size The number of bytes to write.
     * @param buffer Buffer containing data to write.
     */
    void Vision::writeMem(u64 offset, u64 size, const std::vector<char>& buffer) {
        attach();
        Result rc = svcWriteDebugProcessMemory(m_debugHandle, (void*)buffer.data(), offset, size);
        if (R_FAILED(rc)) {
            Logger::logToFile("writedMem() svcWriteDebugProcessMemory() failed.", rc);
        }
        detach();
    }
}
