#include "defines.h"
#include <cstring>
#include "controllerCommands.h"
#include "util.h"
#include "logger.h"

namespace ControllerCommands {
    using namespace Util;
    using namespace SbbLog;

    std::mutex Controller::m_stateMutex;
    Controller::ControllerCommand Controller::m_controllerCommand { 0 };
    Controller::WallClock Controller::m_nextStateChange = WallClock::max();
    std::atomic_bool Controller::m_replaceOnNext { false };

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
        m_hiddbgHdlsState.battery_level = 4; // Set battery charge to full.
        m_hiddbgHdlsState.analog_stick_l.x = 0x0;
        m_hiddbgHdlsState.analog_stick_l.y = -0x0;
        m_hiddbgHdlsState.analog_stick_r.x = 0x0;
        m_hiddbgHdlsState.analog_stick_r.y = -0x0;

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
        std::lock_guard<std::mutex> lock(m_stateMutex);

        initController();
        press(btn);
        release(btn);
    }

    void Controller::press(const HidNpadButton& btn) {
        std::lock_guard<std::mutex> lock(m_stateMutex);

        initController();
        m_hiddbgHdlsState.buttons |= btn;
        Result rc = hiddbgSetHdlsState(m_controllerHandle, &m_hiddbgHdlsState);
        if (R_FAILED(rc)) {
            Logger::logToFile("press() hiddbgSetHdlsState() failed.", rc);
        }
    }

    void Controller::release(const HidNpadButton& btn) {
        std::lock_guard<std::mutex> lock(m_stateMutex);

        initController();
        m_hiddbgHdlsState.buttons &= ~btn;
        Result rc = hiddbgSetHdlsState(m_controllerHandle, &m_hiddbgHdlsState);
        if (R_FAILED(rc)) {
            Logger::logToFile("release() hiddbgSetHdlsState() failed.", rc);
        }
    }

    void Controller::setStickState(const Joystick& stick, int dxVal, int dyVal) {
        std::lock_guard<std::mutex> lock(m_stateMutex);

        initController();
        if (stick == Joystick::Left) {
            m_hiddbgHdlsState.analog_stick_l.x = dxVal;
            m_hiddbgHdlsState.analog_stick_l.y = dyVal;
        }
        else {
            m_hiddbgHdlsState.analog_stick_r.x = dxVal;
            m_hiddbgHdlsState.analog_stick_r.y = dyVal;
        }

        Result rc = hiddbgSetHdlsState(m_controllerHandle, &m_hiddbgHdlsState);
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

    void Controller::cqControllerState(const ControllerCommand& cmd, std::vector<char>& buffer) {
        initController();
        m_controllerCommand.state = cmd.state;

        m_hiddbgHdlsState.buttons = cmd.state.buttons;
        m_hiddbgHdlsState.analog_stick_l.x = cmd.state.left_joystick_x;
        m_hiddbgHdlsState.analog_stick_l.y = cmd.state.left_joystick_y;
        m_hiddbgHdlsState.analog_stick_r.x = cmd.state.right_joystick_x;
        m_hiddbgHdlsState.analog_stick_r.y = cmd.state.right_joystick_y;

        Result rc = hiddbgSetHdlsState(m_controllerHandle, &m_hiddbgHdlsState);
        if (R_FAILED(rc)) {
            Logger::logToFile("cqControllerState() hiddbgSetHdlsState() press failed.", rc);
        }

        if (cmd.state.isNeutral()) {
            m_controllerCommand.seqnum = 0;
            m_nextStateChange = WallClock::max();
        } else {
            m_controllerCommand.seqnum = cmd.seqnum;
            m_nextStateChange += std::chrono::milliseconds(cmd.milliseconds);
        }

        if (cmd.seqnum != 0) {
            std::string res = "cqCommandFinished " + std::to_string(cmd.seqnum);
            buffer.insert(buffer.begin(), res.begin(), res.end());
        }
    }

    void Controller::cqControllerStateClear(const ControllerCommand& cmd, std::vector<char>& buffer) {
        initController();
        m_hiddbgHdlsState.buttons = cmd.state.buttons;
        m_hiddbgHdlsState.analog_stick_l.x = cmd.state.left_joystick_x;
        m_hiddbgHdlsState.analog_stick_l.y = cmd.state.left_joystick_y;
        m_hiddbgHdlsState.analog_stick_r.x = cmd.state.right_joystick_x;
        m_hiddbgHdlsState.analog_stick_r.y = cmd.state.right_joystick_y;

        Result rc = hiddbgSetHdlsState(m_controllerHandle, &m_hiddbgHdlsState);
        if (R_FAILED(rc)) {
            Logger::logToFile("cqControllerStateClear() hiddbgSetHdlsState() clear failed.", rc);
        }

        if (cmd.seqnum != 0) {
            std::string res = "cqCommandFinished " + std::to_string(cmd.seqnum);
            buffer.insert(buffer.begin(), res.begin(), res.end());
        }

        m_controllerCommand.seqnum = 0;
        m_nextStateChange = WallClock::max();
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
