#include "defines.h"
#include "commandUtil.h"

namespace CommandUtil {
    // taken from sys-httpd (thanks jolan!)
    static const HidsysNotificationLedPattern breathingpattern = {
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

    // beeg flash for wireless controller
    static const HidsysNotificationLedPattern flashpattern = {
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

    bool CommandUtils::flashLed() {
        Result rc = hidsysInitialize();
        if (R_FAILED(rc))
            return false;

        sendPatternStatic(&breathingpattern, HidNpadIdType_Handheld); // glow in and out x2 for docked joycons
        sendPatternStatic(&flashpattern, HidNpadIdType_No1); // big hard single glow for wireless/wired joycons or controllers
        hidsysExit();
        return true;
    }

    void CommandUtils::sendPatternStatic(const HidsysNotificationLedPattern* pattern, const HidNpadIdType idType) {
        s32 total_entries;
        HidsysUniquePadId unique_pad_ids[2] = { 0 };

        Result rc = hidsysGetUniquePadsFromNpad(idType, unique_pad_ids, 2, &total_entries);
        if (R_FAILED(rc))
            return; // probably incompatible or no pads connected

        for (int i = 0; i < total_entries; i++)
            hidsysSetNotificationLedPattern(pattern, unique_pad_ids[i]);
    }
}
