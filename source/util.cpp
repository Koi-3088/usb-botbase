#include "defines.h"
#include "logger.h"
#include "util.h"
#include <algorithm>
#include <fstream>
#include <iostream>

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
            return; // probably incompatible or no pads connected
        }

        for (int i = 0; i < total_entries; i++) {
            hidsysSetNotificationLedPattern(pattern, unique_pad_ids[i]);
        }
    }

    bool Utils::isUSB()
    {
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

    void Utils::parseArgs(const std::vector<char>& argstr, std::function<void(const std::string&, const std::vector<std::string>&)> callback) {
        std::string cmdStr(argstr.begin(), argstr.end());
        std::istringstream stream(cmdStr);

        std::vector<std::string> params;
        std::copy(std::istream_iterator<std::string>(stream),
            std::istream_iterator<std::string>(),
            std::back_inserter(params));

        if (params.empty()) {
            callback("", {});
            return;
        }

        std::string command = params[0];
        std::vector<std::string> parameters(params.begin() + 1, params.end());
        parameters.erase(std::remove_if(parameters.begin(), parameters.end(), [](const std::string& s) {
            return s.empty() || s == "\0" || s == "\n";
            }), parameters.end()
        );

        parameters.pop_back();
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
            return std::stoul(arg, NULL, 16);
        }
        return std::stoul(arg, NULL, 10);
    }
}
