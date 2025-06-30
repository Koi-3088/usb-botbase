#include "defines.h"
#include "controllerCommands.h"
#include "util.h"
#include "logger.h"
#include <cstring>

namespace ControllerCommands {
    using namespace Util;
    using namespace SbbLog;
    using namespace LocklessQueue;

    /**
     * @brief Initialize the virtual controller and related state.
     */
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
        m_controllerDevice.npadInterfaceType = HidNpadInterfaceType_Bluetooth;

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

    /**
     * @brief Detach and clean up the virtual controller.
     */
    void Controller::detachController() {
        if (!m_controllerIsInitialised) {
            return;
        }

        Result rc = hiddbgDetachHdlsVirtualDevice(m_controllerHandle);
        if (R_FAILED(rc)) {
            Logger::logToFile("detachController() hiddbgDetachHdlsVirtualDevice() failed.", rc);
        }

        rc = hiddbgReleaseHdlsWorkBuffer(m_sessionId);
        if (R_FAILED(rc)) {
            Logger::logToFile("detachController() hiddbgReleaseHdlsWorkBuffer() failed.", rc);
        }

        hiddbgExit();
        m_sessionId = { 0 };
        m_controllerHandle = { 0 };
        aligned_free(m_workMem);
        m_workMem = nullptr;
        m_controllerIsInitialised = false;
    }

    /**
     * @brief Simulate a button click (press and release).
     * @param btn The button to click.
     */
    void Controller::click(const HidNpadButton& btn) {
        press(btn);
        release(btn);
    }

    /**
     * @brief Simulate a button press.
     * @param btn The button to press.
     */
    void Controller::press(const HidNpadButton& btn) {
        initController();
        m_hiddbgHdlsState.buttons |= btn;
        Result rc = hiddbgSetHdlsState(m_controllerHandle, &m_hiddbgHdlsState);
        if (R_FAILED(rc)) {
            Logger::logToFile("press() hiddbgSetHdlsState() failed.", rc);
        }
    }

    /**
     * @brief Simulate a button release.
     * @param btn The button to release.
     */
    void Controller::release(const HidNpadButton& btn) {
        initController();
        m_hiddbgHdlsState.buttons &= ~btn;
        Result rc = hiddbgSetHdlsState(m_controllerHandle, &m_hiddbgHdlsState);
        if (R_FAILED(rc)) {
            Logger::logToFile("release() hiddbgSetHdlsState() failed.", rc);
        }
    }

    /**
     * @brief Set the state of a joystick.
     * @param stick The joystick (left or right).
     * @param dxVal X value.
     * @param dyVal Y value.
     */
    void Controller::setStickState(const Joystick& stick, int dxVal, int dyVal) {
        initController();
        if (stick == Joystick::Left) {
            m_hiddbgHdlsState.analog_stick_l.x = dxVal;
            m_hiddbgHdlsState.analog_stick_l.y = dyVal;
        } else {
            m_hiddbgHdlsState.analog_stick_r.x = dxVal;
            m_hiddbgHdlsState.analog_stick_r.y = dyVal;
        }

        Result rc = hiddbgSetHdlsState(m_controllerHandle, &m_hiddbgHdlsState);
        if (R_FAILED(rc)) {
            Logger::logToFile("setStickState() hiddbgSetHdlsState() failed.", rc);
        }
    }

    /**
     * @brief Simulate touch input.
     * @param state Touch state array.
     * @param sequentialCount Number of sequential touches.
     * @param holdTime Hold time in microseconds.
     * @param hold Whether to hold the touch.
     */
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

    /**
     * @brief Simulate keyboard input.
     * @param states Keyboard autopilot states.
     * @param sequentialCount Number of sequential key presses.
     */
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
            } else {
                hiddbgSetKeyboardAutoPilotState(&m_dummyKeyboardState);
                svcSleepThread(pollRate * 1e+6L);
            }
        }

        hiddbgUnsetKeyboardAutoPilotState();
    }

    /**
     * @brief Set the controller type from parameters.
     * @param params Parameters vector.
     */
    void Controller::setControllerType(const std::vector<std::string>& params) {
        detachController();
        m_controllerInitializedType = (HidDeviceType)Utils::parseStringToInt(params[0]);
    }

    /**
     * @brief Start the PA controller thread for processing commands.
     * @param senderQueue Queue for sending data.
     * @param senderCv Condition variable for the sender queue.
     */
    void Controller::startControllerThread(LockFreeQueue<std::vector<char>>& senderQueue, std::condition_variable& senderCv) {
        if (m_ccThreadRunning) {
            Logger::logToFile("Controller thread already running.");
            return;
        }

        Logger::logToFile("Starting commandLoopPA thread.");
        m_ccThread = std::thread(&Controller::commandLoopPA, this, std::ref(senderQueue), std::ref(senderCv));
    }

    /**
     * @brief Main loop for processing PA controller commands in a thread.
     * @param senderQueue Queue for sending data.
     * @param senderCv Condition variable for the sender queue.
     */
    void Controller::commandLoopPA(LockFreeQueue<std::vector<char>>& senderQueue, std::condition_variable& senderCv) {
        auto cmd = ControllerCommand {};
        const std::chrono::microseconds EARLY_WAKE(1000);
        m_ccThreadRunning = true;
        Logger::logToFile("commandLoopPA() started.");

        while (!m_error) {
            WallClock now = std::chrono::steady_clock::now();
            {
                std::unique_lock<std::mutex> lock(m_ccMutex);
                if (now >= m_nextStateChange.load(std::memory_order_acquire)) {
                    if (!m_ccQueue.empty()) {
                        Logger::logToFile("commandLoopPA() processing command.");
                        m_ccQueue.pop(cmd);
                        cqSendState(cmd, senderQueue, senderCv);
                        m_nextStateChange.store(now + std::chrono::milliseconds(cmd.milliseconds), std::memory_order_release);
                    } else {
                        Logger::logToFile("commandLoopPA() clearing state due to empty queue.");
                        cmd.state.clear();
                        cqSendState(cmd, senderQueue, senderCv);
                        cmd.seqnum = 0;
                        m_nextStateChange.store(WallClock::max(), std::memory_order_release);
                    }
                }
            }

            std::unique_lock<std::mutex> lock(m_ccMutex);
            m_ccCv.wait_until(lock, WallClock(m_nextStateChange.load(std::memory_order_acquire) - WallClock(std::chrono::steady_clock::now() - EARLY_WAKE)), [&] {
                return !m_ccQueue.empty() ||
                    m_error.load(std::memory_order_acquire) ||
                    std::chrono::steady_clock::now() + EARLY_WAKE >= m_nextStateChange.load(std::memory_order_acquire);
            });
        }

        cmd = ControllerCommand {};
        cqSendState(cmd, senderQueue, senderCv);
        detachController();
        m_ccThreadRunning = false;
        m_error = true;
        Logger::logToFile("commandLoopPA() stopped thread.");
    }

    /**
     * @brief Sends the current controller state to the client.
     * @param cmd         The controller command (not used directly; m_controllerCommand is used).
     * @param senderQueue The queue to which the serialized state will be pushed.
     * @param senderCv    The condition variable to notify after pushing to the queue.
     */
    void Controller::cqSendState(const ControllerCommand& cmd, LockFreeQueue<std::vector<char>>& senderQueue, std::condition_variable& senderCv) {
        cqControllerState(cmd);
        if (cmd.seqnum != 0) {
            Logger::logToFile("cqSendState() command finished with seqnum: " + std::to_string(cmd.seqnum));
            std::string res = "cqCommandFinished " + std::to_string(cmd.seqnum) + "\r\n";
            senderQueue.push(std::vector<char>(res.begin(), res.end()));
            senderCv.notify_one();
        }
    }

    /**
     * @brief Update the PA controller state and optionally send a response.
     * @param cmd The PA controller command.
     */
    void Controller::cqControllerState(const ControllerCommand& cmd) {
        std::lock_guard<std::mutex> lock(m_controllerMutex);
        Logger::logToFile("cqControllerState() called with seqnum: " + std::to_string(cmd.seqnum));
        initController();

        m_hiddbgHdlsState.buttons = cmd.state.buttons;
        m_hiddbgHdlsState.analog_stick_l.x = cmd.state.left_joystick_x;
        m_hiddbgHdlsState.analog_stick_l.y = cmd.state.left_joystick_y;
        m_hiddbgHdlsState.analog_stick_r.x = cmd.state.right_joystick_x;
        m_hiddbgHdlsState.analog_stick_r.y = cmd.state.right_joystick_y;

        Result rc = hiddbgSetHdlsState(m_controllerHandle, &m_hiddbgHdlsState);
        if (R_FAILED(rc)) {
            Logger::logToFile("cqControllerState() hiddbgSetHdlsState() failed.", rc);
        }
    }

    /**
     * @brief Enqueue a PA controller command for processing.
     * @param cmd The controller command.
     * @param replace Whether to replace the current command queue.
     * @param cancel Whether to cancel the current command queue.
     */
    void Controller::cqEnqueueCommand(const ControllerCommand& cmd, const bool& replace, const bool& cancel) {
        std::lock_guard<std::mutex> lock(m_enqueueMutex);
        if (m_ccQueue.full()) {
            return;
        }

        if (replace) {
            std::lock_guard<std::mutex> lock(m_ccMutex);
            Logger::logToFile("cqEnqueueCommand() replace.");
            m_nextStateChange.store(WallClock::max(), std::memory_order_release);
            m_ccQueue.clear();
            m_ccCv.notify_one();
            return;
        }

        if (cancel) {
            std::lock_guard<std::mutex> lock(m_ccMutex);
            Logger::logToFile("cqEnqueueCommand() cancel.");
            m_ccQueue.clear();
            cqControllerState(cmd);
            m_nextStateChange.store(WallClock::max(), std::memory_order_release);
            m_ccCv.notify_one();
            return;
        }

        Logger::logToFile("cqEnqueueCommand() pushing command with seqnum: " + std::to_string(cmd.seqnum));
        m_ccQueue.push(cmd);
        if (m_nextStateChange.load(std::memory_order_acquire) == WallClock::max()) {
            m_nextStateChange.store(WallClock::min(), std::memory_order_release);
        }

        m_ccCv.notify_one();
    }

    /**
     * @brief Notify all PA threads.
     */
    void Controller::cqNotifyAll() {
        std::lock_guard<std::mutex> lock(m_ccMutex);
        m_error.store(true, std::memory_order_release);
        m_ccCv.notify_one();
    }

    /**
     * @brief Parse a string to a button value.
     * @param arg The string argument.
     * @return The button value, or -1 if not found.
     */
    int Controller::parseStringToButton(const std::string& arg) {
        auto it = Controller::m_button.find(arg);
        if (it != Controller::m_button.end()) {
            return it->second;
        } else {
            Logger::logToFile("parseStringToButton() button not found (" + arg + ").");
            return -1;
        }
    }

    /**
     * @brief Parse a string to a stick value.
     * @param arg The string argument.
     * @return The stick value, or -1 if not found.
     */
    int Controller::parseStringToStick(const std::string& arg) {
        auto it = Controller::m_stick.find(arg);
        if (it != Controller::m_stick.end()) {
            return it->second;
        } else {
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
