#include "defines.h"
#include "controllerCommands.h"
#include "logger.h"
#include "util.h"
#include <switch.h>

namespace ControllerCommands {
    using namespace Util;
    using namespace SbbLog;

    void Controller::initController() {
        if (bControllerIsInitialised) {
            return;
        }

        //taken from switchexamples github
        Result rc = hiddbgInitialize();
        if (R_FAILED(rc)) {
            Logger::logToFile("initController() hiddbgInitialize() failed.");
        }

        if (m_workMem == nullptr) {
            m_workMem = (u8*)aligned_alloc(0x1000, m_workMem_size);
        }

        if (m_workMem) {
            Logger::logToFile("initController() aligned_alloc() success.");
        }
        else {
            Logger::logToFile("initController() aligned_alloc() failed.");
        }

        // Set the controller type to Pro-Controller, and set the npadInterfaceType.
        controllerDevice.deviceType = HidDeviceType_FullKey3;
        controllerDevice.npadInterfaceType = HidNpadInterfaceType_Bluetooth;
        // Set the controller colors. The grip colors are for Pro-Controller on [9.0.0+].
        controllerDevice.singleColorBody = RGBA8_MAXALPHA(255, 255, 255);
        controllerDevice.singleColorButtons = RGBA8_MAXALPHA(0, 0, 0);
        controllerDevice.colorLeftGrip = RGBA8_MAXALPHA(230, 255, 0);
        controllerDevice.colorRightGrip = RGBA8_MAXALPHA(0, 40, 20);

        // Setup example controller state.
        controllerState.battery_level = 4; // Set battery charge to full.
        controllerState.analog_stick_l.x = 0x0;
        controllerState.analog_stick_l.y = -0x0;
        controllerState.analog_stick_r.x = 0x0;
        controllerState.analog_stick_r.y = -0x0;

        rc = hiddbgAttachHdlsWorkBuffer(&sessionId, m_workMem, m_workMem_size);
        if (R_FAILED(rc)) {
            Logger::logToFile("initController() hiddbgAttachHdlsWorkBuffer() failed.");
        }

        rc = hiddbgAttachHdlsVirtualDevice(&controllerHandle, &controllerDevice);
        if (R_FAILED(rc)) {
            Logger::logToFile("initController() hiddbgAttachHdlsVirtualDevice() failed.");
        }

        //init a dummy keyboard state for assignment between keypresses
        dummyKeyboardState.keys[3] = 0x800000000000000UL; // Hackfix found by Red: an unused key press (KBD_MEDIA_CALC) is required to allow sequential same-key presses. bitfield[3]
        bControllerIsInitialised = true;
    }

    void Controller::detachController() {
        initController();

        Result rc = hiddbgDetachHdlsVirtualDevice(controllerHandle);
        if (R_FAILED(rc)) {
            Logger::logToFile("detachController() hiddbgDetachHdlsVirtualDevice() failed.");
        }

        rc = hiddbgReleaseHdlsWorkBuffer(sessionId);
        if (R_FAILED(rc)) {
            Logger::logToFile("detachController() hiddbgReleaseHdlsWorkBuffer() failed.");
        }

        hiddbgExit();
        if (m_workMem != nullptr) {
            free(m_workMem);
            m_workMem = nullptr;
        }

        sessionId.id = 0;
        bControllerIsInitialised = false;
    }

    void Controller::click(HidNpadButton btn) {
        initController();
        press(btn);
        svcSleepThread(buttonClickSleepTime * 1e+6L);
        release(btn);
    }

    void Controller::press(HidNpadButton btn) {
        initController();
        controllerState.buttons |= btn;
        Result rc = hiddbgSetHdlsState(controllerHandle, &controllerState);
        if (R_FAILED(rc)) {
            Logger::logToFile("press() hiddbgSetHdlsState() failed.");
        }
    }

    void Controller::release(HidNpadButton btn) {
        initController();
        controllerState.buttons &= ~btn;
        Result rc = hiddbgSetHdlsState(controllerHandle, &controllerState);
        if (R_FAILED(rc)) {
            Logger::logToFile("release() hiddbgSetHdlsState() failed.");
        }
    }

    void Controller::setStickState(Controller::Joystick stick, int dxVal, int dyVal) {
        initController();
        if (stick == Joystick::Left) {
            controllerState.analog_stick_l.x = dxVal;
            controllerState.analog_stick_l.y = dyVal;
        }
        else {
            controllerState.analog_stick_r.x = dxVal;
            controllerState.analog_stick_r.y = dyVal;
        }

        Result rc = hiddbgSetHdlsState(controllerHandle, &controllerState);
        if (R_FAILED(rc)) {
            Logger::logToFile("setStickState() hiddbgSetHdlsState() failed.");
        }
    }

    void Controller::touch(HidTouchState* state, u64 sequentialCount, u64 holdTime, bool hold, u8* token) {
        initController();
        state->delta_time = holdTime; // only the first touch needs this for whatever reason
        for (u32 i = 0; i < sequentialCount; i++) {
            hiddbgSetTouchScreenAutoPilotState(&state[i], 1);
            svcSleepThread(holdTime);
            if (!hold) {
                hiddbgSetTouchScreenAutoPilotState(NULL, 0);
                svcSleepThread(pollRate * 1e+6L);
            }

            if ((*token) == 1) {
                break;
            }
        }

        if (hold) { // send finger release event
            hiddbgSetTouchScreenAutoPilotState(NULL, 0);
            svcSleepThread(pollRate * 1e+6L);
        }

        hiddbgUnsetTouchScreenAutoPilotState();
    }

    void Controller::key(HiddbgKeyboardAutoPilotState* states, u64 sequentialCount) {
        initController();
        HiddbgKeyboardAutoPilotState tempState = { 0 };
        u32 i;
        for (i = 0; i < sequentialCount; i++) {
            memcpy(&tempState.keys, states[i].keys, sizeof(u64) * 4);
            tempState.modifiers = states[i].modifiers;
            hiddbgSetKeyboardAutoPilotState(&tempState);
            svcSleepThread(keyPressSleepTime * 1e+6L);

            if (i != (sequentialCount - 1)) {
                if (memcmp(states[i].keys, states[i + 1].keys, sizeof(u64) * 4) == 0 && states[i].modifiers == states[i + 1].modifiers) {
                    hiddbgSetKeyboardAutoPilotState(&dummyKeyboardState);
                    svcSleepThread(pollRate * 1e+6L);
                }
            }
            else {
                hiddbgSetKeyboardAutoPilotState(&dummyKeyboardState);
                svcSleepThread(pollRate * 1e+6L);
            }
        }

        hiddbgUnsetKeyboardAutoPilotState();
    }

    // Direct copy-paste with conflicts commented out. Need to rewrite, test, fix.
    void Controller::clickSequence(char* seq, u8* token) {
        const char delim = ','; // used for chars and sticks
        const char startWait = 'W';
        const char startPress = '+';
        const char startRelease = '-';
        const char startLStick = '%';
        const char startRStick = '&';
        char* command = strtok(seq, &delim);
        HidNpadButton currKey = {};
        u64 currentWait = 0;

        initController();
        while (command != NULL) {
            if ((*token) == 1) {
                break;
            }

            if (!strncmp(command, &startLStick, 1)) {
                // l stick
                s64 x = Utils::parseStringToSignedLong(&command[1]);
                if (x > JOYSTICK_MAX) {
                    x = JOYSTICK_MAX;
                }

                if (x < JOYSTICK_MIN) {
                    x = JOYSTICK_MIN;
                }

                s64 y = 0;
                command = strtok(NULL, &delim);
                if (command != NULL) {
                    y = Utils::parseStringToSignedLong(command);
                }

                if (y > JOYSTICK_MAX) {
                    y = JOYSTICK_MAX;
                }

                if (y < JOYSTICK_MIN) {
                    y = JOYSTICK_MIN;
                }

                setStickState(Joystick::Left, (s32)x, (s32)y);
            }
            else if (!strncmp(command, &startRStick, 1)) {
                // r stick
                s64 x = Utils::parseStringToSignedLong(&command[1]);
                if (x > JOYSTICK_MAX) {
                    x = JOYSTICK_MAX;
                }

                if (x < JOYSTICK_MIN) {
                    x = JOYSTICK_MIN;
                }

                s64 y = 0;
                command = strtok(NULL, &delim);
                if (command != NULL) {
                    y = Utils::parseStringToSignedLong(command);
                }

                if (y > JOYSTICK_MAX) {
                    y = JOYSTICK_MAX;
                }

                if (y < JOYSTICK_MIN) {
                    y = JOYSTICK_MIN;
                }

                setStickState(Joystick::Right, (s32)x, (s32)y);
            }
            else if (!strncmp(command, &startPress, 1)) {
                // press
                currKey = (HidNpadButton)parseStringToButton(&command[1]);
                press(currKey);
            }
            else if (!strncmp(command, &startRelease, 1)) {
                // release
                currKey = (HidNpadButton)parseStringToButton(&command[1]);
                release(currKey);
            }
            else if (!strncmp(command, &startWait, 1)) {
                // wait
                currentWait = Utils::parseStringToInt(&command[1]);
                svcSleepThread(currentWait * 1e+6l);
            }
            else {
                // click
                currKey = (HidNpadButton)parseStringToButton(command);
                press(currKey);
                svcSleepThread(buttonClickSleepTime * 1e+6L);
                release(currKey);
            }

            command = strtok(NULL, &delim);
        }
    }

    int Controller::parseStringToButton(const std::string& arg) {
        auto it = Controller::m_button.find(arg);
        if (it != Controller::m_button.end()) {
            return it->second;
        }
        else {
            Logger::logToFile("parseStringToButton() button not found (" + arg + ").");
            return 0;
        }
    }

    std::unordered_map<std::string, int> Controller::m_button {
            { "A", HidNpadButton_A },
            { "B", HidNpadButton_B },
            { "X", HidNpadButton_X },
            { "Y", HidNpadButton_Y },
            { "RSTICK", HidNpadButton_StickR },
            { "LSTICK", HidNpadButton_StickL },
            { "L", HidNpadButton_L },
            { "R", HidNpadButton_R },
            { "ZL", HidNpadButton_ZL },
            { "ZR", HidNpadButton_ZR },
            { "PLUS", HidNpadButton_Plus },
            { "MINUS", HidNpadButton_Minus },
            { "DLEFT", HidNpadButton_Left },
            { "DL", HidNpadButton_Left },
            { "DUP", HidNpadButton_Up },
            { "DU", HidNpadButton_Up },
            { "DRIGHT", HidNpadButton_Right },
            { "DR", HidNpadButton_Right },
            { "DDOWN", HidNpadButton_Down },
            { "DD", HidNpadButton_Down },
            { "HOME", BIT(18) }, // HiddbgNpadButton_HOME
            { "CAPTURE", BIT(19) }, // HiddbgNpadButton_CAPTURE
            { "PALMA", HidNpadButton_Palma },
            { "UNUSED", BIT(20)},
    };
}
