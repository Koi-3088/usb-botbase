#include "defines.h"
#include "logger.h"
#include "util.h"
#include <cstring>
#include <fstream>
#include <sstream>
#include <switch.h>

namespace Util {
    using namespace SbbLog;

    // taken from sys-httpd (thanks jolan!)
    static const HidsysNotificationLedPattern breathingPattern = {
        0x8,       // 100ms (baseMiniCycleDuration)
        0x2,       // 3 mini cycles. Last one 12.5ms (totalMiniCycles)
        0x2,       // 2 full cycles (totalFullCycles)
        0x2,       // 13% (startIntensity)
        {   // miniCycles
            // First cycle
            {
                0xF,      // 100% (ledIntensity)
                0xF,      // 15 steps. Transition time 1.5s (transitionSteps)
                0x0,      // 12.5ms (finalStepDuration)
            },
            // Second cycle
            {
                0x2,      // 13% (ledIntensity)
                0xF,      // 15 steps. Transition time 1.5s (transitionSteps)
                0x0,      // 12.5ms (finalStepDuration)
            },
        },
    };

    static const HidsysNotificationLedPattern flashPattern = {
        0xF,       // 200ms (baseMiniCycleDuration)
        0x2,       // 3 mini cycles. Last one 12.5ms (totalMiniCycles)
        0x2,       // 2 full cycles (totalFullCycles)
        0xF,       // 100% (startIntensity)
        {   // miniCycles
            // First and cloned cycle
            {
                0xF,      // 100% (ledIntensity)
                0xF,      // 15 steps. Transition time 1.5s (transitionSteps)
                0x0,      // 12.5ms (finalStepDuration)
            },
            // clone
            {
                0xF,      // 100% (ledIntensity)
                0xF,      // 15 steps. Transition time 1.5s (transitionSteps)
                0x0,      // 12.5ms (finalStepDuration)
            },
        },
    };

    bool Utils::flashLed() {
        Result rc = hidsysInitialize();
        if (R_FAILED(rc)) {
            Logger::instance().log("flashLed() hidsysInitialize() failed.", std::to_string(R_DESCRIPTION(rc)));
            return false;
        }

        sendPatternStatic(&breathingPattern, HidNpadIdType_Handheld); // glow in and out x2 for docked joycons
        sendPatternStatic(&flashPattern, HidNpadIdType_No1); // big hard single glow for wireless/wired joycons or controllers
        hidsysExit();
        return true;
    }

    void Utils::sendPatternStatic(const HidsysNotificationLedPattern* pattern, const HidNpadIdType idType) {
        s32 total_entries;
        HidsysUniquePadId unique_pad_ids[2] = { 0 };

        Result rc = hidsysGetUniquePadsFromNpad(idType, unique_pad_ids, 2, &total_entries);
        if (R_FAILED(rc)) {
            Logger::instance().log("sendPatternStatic() hidsysGetUniquePadsFromNpad() failed.", std::to_string(R_DESCRIPTION(rc)));
            return;
        }

        for (int i = 0; i < total_entries; i++) {
            hidsysSetNotificationLedPattern(pattern, unique_pad_ids[i]);
        }
    }

    bool Utils::isUSB() {
        std::vector<char> str(10);
        std::ifstream cfg("sdmc:/atmosphere/contents/430000000000000B/config.cfg");
        if (cfg.is_open()) {
            cfg.getline(str.data(), str.size(), '\n');
            cfg.close();

            str.shrink_to_fit();
            if (std::string(str.data()) == "usb") {
                return true;
            }
        }

        cfg.close();
        return false;
    }

    void Utils::parseArgs(const std::string& cmd, std::function<void(const std::string&, const std::vector<std::string>&)> callback) {
        std::vector<std::string> params;
        size_t start = 0, end = 0, len = cmd.length();

        while (start < len) {
            while (start < len && std::isspace(cmd[start])) {
                ++start;
            }

            if (start >= len) {
                break;
            }

            end = start;
            while (end < len && !std::isspace(cmd[end])) {
                ++end;
            }

            std::string token = cmd.substr(start, end - start);
            if (!token.empty() && token != "\n") {
                params.push_back(token);
            }

            start = end;
        }

        if (params.empty()) {
            return;
        }

        std::string command = params[0];
        std::vector<std::string> parameters;
        if (params.size() > 1) {
            parameters.assign(params.begin() + 1, params.end());
        }

        callback(command, parameters);
    }

    u64 Utils::parseStringToInt(const std::string& arg) {
        if (arg.length() > 2 && arg[1] == 'x') {
            return std::stoull(arg, NULL, 16);
        }

        return std::stoull(arg, NULL, 10);
    }

    s64 Utils::parseStringToSignedLong(const std::string& arg) {
        if (arg.length() > 2 && (arg[1] == 'x' || arg[2] == 'x')) {
            return std::stoll(arg, NULL, 16);
        }

        return std::stoll(arg, NULL, 10);
    }

    std::vector<char> Utils::parseStringToByteBuffer(const std::string& arg) {
        std::string argStr = arg;
        char toTranslate[3] = { 0 };
        int length = argStr.length();
        bool isHex = false;

        if (length > 2) {
            if (argStr[1] == 'x') {
                isHex = true;
                length -= 2;
                argStr = &argStr[2]; //cut off 0x
            }
        }

        bool isFirst = true;
        bool isOdd = (length % 2 == 1);
        u64 bufferSize = length / 2;
        if (isOdd) {
            bufferSize++;
        }

        std::vector<char> buffer;
        for (u64 i = 0; i < bufferSize; i++) {
            if (isOdd) {
                if (isFirst) {
                    toTranslate[0] = '0';
                    toTranslate[1] = argStr[i];
                }
                else {
                    toTranslate[0] = argStr[(2 * i) - 1];
                    toTranslate[1] = argStr[(2 * i)];
                }
            }
            else {
                toTranslate[0] = argStr[i * 2];
                toTranslate[1] = argStr[(i * 2) + 1];
            }

            isFirst = false;
            if (isHex) {
                buffer.push_back(std::stoull(toTranslate, NULL, 16));
            }
            else {
                buffer.push_back(std::stoull(toTranslate, NULL, 10));
            }
        }

        return buffer;
    }

    /**
     * @brief Handle backwards compatibility for sent data by WiFi.
     * @param Output buffer for result.
     * @param If true, the bytes will be flipped in the output buffer.
     */
    void Utils::hexify(std::vector<char>& buffer, bool flip) {
        if (buffer.empty()) {
            return;
        }

        std::vector<char> hexBuffer;
        hexBuffer.reserve(buffer.size() * 2);

        static const char hexDigits[] = "0123456789ABCDEF";
        size_t n = buffer.size();

        if (flip) {
            for (size_t i = 0; i < n; ++i) {
                unsigned char c = static_cast<unsigned char>(buffer[n - 1 - i]);
                hexBuffer.push_back(hexDigits[(c >> 4) & 0xF]);
                hexBuffer.push_back(hexDigits[c & 0xF]);
            }
        } else {
            for (size_t i = 0; i < n; ++i) {
                unsigned char c = static_cast<unsigned char>(buffer[i]);
                hexBuffer.push_back(hexDigits[(c >> 4) & 0xF]);
                hexBuffer.push_back(hexDigits[c & 0xF]);
            }
        }

        buffer.swap(hexBuffer);
    }

    /**
     * @brief Handle backwards compatibility for sent strings by WiFi.
     * @param Output buffer for result.
     * @param If true, the bytes will be flipped in the output buffer.
     */
    void Utils::hexifyString(std::vector<char>& buffer, bool flip) {
        if (buffer.empty()) {
            return;
        }

        size_t valueSize = buffer.size();
        char hexStr[17] = { 0 };
        const char* dataPtr = buffer.data();
        char temp[8];

        if (flip && valueSize > 1 && valueSize <= 8) {
            for (size_t i = 0; i < valueSize; ++i) {
                temp[i] = buffer[valueSize - 1 - i];
            }

            dataPtr = temp;
        }

        switch (valueSize) {
        case 8: {
            uint64_t value;
            std::memcpy(&value, dataPtr, 8);
            std::snprintf(hexStr, sizeof(hexStr), "%016llX", static_cast<unsigned long long>(value));
            buffer.assign(hexStr, hexStr + 16);
            break;
        }
        case 4: {
            uint32_t value;
            std::memcpy(&value, dataPtr, 4);
            std::snprintf(hexStr, sizeof(hexStr), "%08X", value);
            buffer.assign(hexStr, hexStr + 8);
            break;
        }
        case 2: {
            uint16_t value;
            std::memcpy(&value, dataPtr, 2);
            std::snprintf(hexStr, sizeof(hexStr), "%04X", value);
            buffer.assign(hexStr, hexStr + 4);
            break;
        }
        case 1: {
            uint8_t value = static_cast<uint8_t>(dataPtr[0]);
            std::snprintf(hexStr, sizeof(hexStr), "%02X", value);
            buffer.assign(hexStr, hexStr + 2);
            break;
        }
        default:
            Logger::instance().log("hexifyString() Unsupported buffer size: " + std::to_string(valueSize), "", true);
            break;
        }
    }
}
