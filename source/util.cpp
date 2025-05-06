#include "defines.h"
#include "util.h"
#include <iterator>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <switch.h>

namespace Util {
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
            return;
        }

        for (int i = 0; i < total_entries; i++) {
            hidsysSetNotificationLedPattern(pattern, unique_pad_ids[i]);
        }
    }

    bool Utils::isUSB() {
        char str[10];
        std::ifstream cfg("sdmc:/atmosphere/contents/430000000000000B/config.cfg");
        if (cfg.is_open()) {
            cfg.getline(str, 10, '\n');
            cfg.close();

            if (std::string(str) == "usb") {
                return true;
            }
        }

        cfg.close();
        return false;
    }

    void Utils::parseArgs(const std::string& cmd, std::function<void(const std::string&, const std::vector<std::string>&)> callback) {
        std::istringstream stream(cmd);

        std::vector<std::string> params;
        std::copy(std::istream_iterator<std::string>(stream),
            std::istream_iterator<std::string>(),
            std::back_inserter(params));

        if (params.empty()) {
            //Logger::logToFile("parseArgs() params empty.");
            return;
        }

        std::string command = params[0];
        if (params.size() > 1) {
            std::vector<std::string> parameters(params.begin() + 1, params.end());
            parameters.erase(std::remove_if(parameters.begin(), parameters.end(), [](const std::string& s) {
                return s == "\0" || s == "\n" || s.empty();
                }), parameters.end()
            );

            callback(command, parameters);
            return;
        }

        callback(command, {});
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
        char toTranslate[3] = { 0 };
        int length = arg.length();
        bool isHex = false;
        std::string argStr = arg;

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

        std::vector<char> buffer(bufferSize);
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
                buffer[i] = std::stoull(toTranslate, NULL, 16);
            }
            else {
                buffer[i] = std::stoull(toTranslate, NULL, 10);
            }
        }

        return buffer;
    }
}
