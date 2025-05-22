#include "defines.h"
#include <cstring>
#include "controllerCommands.h"
#include "util.h"
#include "logger.h"

namespace ControllerCommands {
    using namespace Util;
    using namespace SbbLog;

    void Controller::initController() {
        if (m_controllerIsInitialised) {
            return;
        }

        //taken from switchexamples github
        Result rc = hiddbgInitialize();
        if (R_FAILED(rc)) {
            Logger::logToFile("initController() hiddbgInitialize() failed.", rc);
            return;
        }
        
        if (!m_workMem) {
            m_workMem = (u8*)aligned_alloc(0x1000, m_workMem_size);
            if (!m_workMem) {
                Logger::logToFile("initController() aligned_alloc() failed.");
                return;
            }
        }

        // Set the controller type to Pro-Controller, and set the npadInterfaceType.
        m_controllerDevice.deviceType = HidDeviceType_FullKey3;
        m_controllerDevice.npadInterfaceType = HidNpadInterfaceType_USB;
        // Set the controller colors. The grip colors are for Pro-Controller on [9.0.0+].
        m_controllerDevice.singleColorBody = RGBA8_MAXALPHA(0, 0, 0);
        m_controllerDevice.singleColorButtons = RGBA8_MAXALPHA(255, 255, 255);
        m_controllerDevice.colorLeftGrip = RGBA8_MAXALPHA(0, 0, 255);
        m_controllerDevice.colorRightGrip = RGBA8_MAXALPHA(0, 255, 0);

        // Setup example controller state.
        m_controllerState.battery_level = 4; // Set battery charge to full.
        m_controllerState.analog_stick_l.x = 0x0;
        m_controllerState.analog_stick_l.y = -0x0;
        m_controllerState.analog_stick_r.x = 0x0;
        m_controllerState.analog_stick_r.y = -0x0;

        rc = hiddbgAttachHdlsWorkBuffer(&m_sessionId, m_workMem, m_workMem_size);
        if (R_FAILED(rc)) {
            Logger::logToFile("initController() hiddbgAttachHdlsWorkBuffer() failed.", rc);
        }

        rc = hiddbgAttachHdlsVirtualDevice(&m_controllerHandle, &m_controllerDevice);
        if (R_FAILED(rc)) {
            Logger::logToFile("initController() hiddbgAttachHdlsVirtualDevice() failed.", rc);
        }

        //init a dummy keyboard state for assignment between keypresses
        // Hackfix found by Red: an unused key press (KBD_MEDIA_CALC) is required to allow sequential same-key presses. bitfield[3]
        m_dummyKeyboardState.keys[3] = 0x800000000000000UL;
        m_controllerIsInitialised = true;
    }

    void Controller::detachController() {
        initController();

        Result rc = hiddbgDetachHdlsVirtualDevice(m_controllerHandle);
        if (R_FAILED(rc)) {
            Logger::logToFile("detachController() hiddbgDetachHdlsVirtualDevice() failed.", rc);
        }

        rc = hiddbgReleaseHdlsWorkBuffer(m_sessionId);
        if (R_FAILED(rc)) {
            Logger::logToFile("detachController() hiddbgReleaseHdlsWorkBuffer() failed.", rc);
        }

        hiddbgExit();
        if (m_workMem != nullptr) {
            aligned_free(m_workMem);
            m_workMem = nullptr;
        }

        m_sessionId.id = 0;
        m_controllerIsInitialised = false;
    }

    void Controller::click(const HidNpadButton& btn) {
        initController();
        press(btn);
        release(btn);
    }

    void Controller::press(const HidNpadButton& btn) {
        initController();
        m_controllerState.buttons |= btn;
        Result rc = hiddbgSetHdlsState(m_controllerHandle, &m_controllerState);
        if (R_FAILED(rc)) {
            Logger::logToFile("press() hiddbgSetHdlsState() failed.", rc);
        }
    }

    void Controller::release(const HidNpadButton& btn) {
        initController();
        m_controllerState.buttons &= ~btn;
        Result rc = hiddbgSetHdlsState(m_controllerHandle, &m_controllerState);
        if (R_FAILED(rc)) {
            Logger::logToFile("release() hiddbgSetHdlsState() failed.", rc);
        }
    }

    void Controller::setStickState(const Joystick& stick, int dxVal, int dyVal) {
        initController();
        if (stick == Joystick::Left) {
            m_controllerState.analog_stick_l.x = dxVal;
            m_controllerState.analog_stick_l.y = dyVal;
        }
        else {
            m_controllerState.analog_stick_r.x = dxVal;
            m_controllerState.analog_stick_r.y = dyVal;
        }

        Result rc = hiddbgSetHdlsState(m_controllerHandle, &m_controllerState);
        if (R_FAILED(rc)) {
            Logger::logToFile("setStickState() hiddbgSetHdlsState() failed.", rc);
        }
    }

    void Controller::touch(std::vector<HidTouchState>& state, u64 sequentialCount, u64 holdTime, bool hold) {
        initController();
        state[0].delta_time = holdTime; // only the first touch needs this for whatever reason
        for (u32 i = 0; i < sequentialCount; i++) {
            hiddbgSetTouchScreenAutoPilotState(&state[i], 1);
            svcSleepThread(holdTime);
            if (!hold) {
                hiddbgSetTouchScreenAutoPilotState(NULL, 0);
                svcSleepThread(pollRate * 1e+6L);
            }
        }

        // send finger release event
        if (hold) {
            hiddbgSetTouchScreenAutoPilotState(NULL, 0);
            svcSleepThread(pollRate * 1e+6L);
        }

        hiddbgUnsetTouchScreenAutoPilotState();
    }

    void Controller::key(const std::vector<HiddbgKeyboardAutoPilotState>& states, u64 sequentialCount) {
        initController();
        HiddbgKeyboardAutoPilotState tempState = { 0 };
        u32 i;
        for (i = 0; i < sequentialCount; i++) {
            std::memcpy(&tempState.keys, states[i].keys, sizeof(u64) * 4);
            tempState.modifiers = states[i].modifiers;
            hiddbgSetKeyboardAutoPilotState(&tempState);
            svcSleepThread(keyPressSleepTime * 1e+6L);

            if (i != (sequentialCount - 1)) {
                if (std::memcmp(states[i].keys, states[i + 1].keys, sizeof(u64) * 4) == 0 && states[i].modifiers == states[i + 1].modifiers) {
                    hiddbgSetKeyboardAutoPilotState(&m_dummyKeyboardState);
                    svcSleepThread(pollRate * 1e+6L);
                }
            }
            else {
                hiddbgSetKeyboardAutoPilotState(&m_dummyKeyboardState);
                svcSleepThread(pollRate * 1e+6L);
            }
        }

        hiddbgUnsetKeyboardAutoPilotState();
    }

    void Controller::setControllerType(const std::vector<std::string>& params) {
        detachController();
        m_controllerInitializedType = (HidDeviceType)Utils::parseStringToInt(params[0]);
    }

    void Controller::CcClick(const Command& cmd, std::vector<char>& buffer) {
        initController();

        buffer.resize(sizeof(uint64_t));
        std::copy(reinterpret_cast<const char*>(&cmd.seqnum),
            reinterpret_cast<const char*>(&cmd.seqnum + sizeof(uint64_t)),
            buffer.begin());

        m_controllerState.buttons |= cmd.buttons;
        m_controllerState.analog_stick_l.x = cmd.left_joystick_x;
        m_controllerState.analog_stick_l.y = cmd.left_joystick_y;
        m_controllerState.analog_stick_r.x = cmd.right_joystick_x;
        m_controllerState.analog_stick_r.y = cmd.right_joystick_y;

        Result rc = hiddbgSetHdlsState(m_controllerHandle, &m_controllerState);
        if (R_FAILED(rc)) {
            Logger::logToFile("handleCcCommand() hiddbgSetHdlsState() press failed.", rc);
        }

        m_controllerState.buttons &= ~cmd.buttons;
        rc = hiddbgSetHdlsState(m_controllerHandle, &m_controllerState);
        if (R_FAILED(rc)) {
            Logger::logToFile("andleCcCommand() hiddbgSetHdlsState() release failed.", rc);
        }
    }

    int Controller::parseStringToButton(const std::string& arg) {
        auto it = Controller::m_button.find(arg);
        if (it != Controller::m_button.end()) {
            return it->second;
        }
        else {
            Logger::logToFile("parseStringToButton() button not found (" + arg + ").");
            return -1;
        }
    }

    int Controller::parseStringToStick(const std::string& arg) {
        auto it = Controller::m_stick.find(arg);
        if (it != Controller::m_stick.end()) {
            return it->second;
        }
        else {
            Logger::logToFile("parseStringToStick() stick not found (" + arg + ").");
            return -1;
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

    std::unordered_map<std::string, int> Controller::m_stick {
        { "LEFT", Joystick::Left },
        { "RIGHT", Joystick::Right },
    };
}
