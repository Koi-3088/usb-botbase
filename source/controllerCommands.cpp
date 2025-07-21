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
            Logger::instance().log("initController() hiddbgInitialize() failed.", std::to_string(R_DESCRIPTION(rc)));
            return;
        }

        if (!m_workMem) {
            m_workMem = (u8*)aligned_alloc(0x1000, m_workMem_size);
            if (!m_workMem) {
                Logger::instance().log("Failed to initialize virtual controller.", "initController() aligned_alloc() failed.");
                return;
            }
        }

        // Set the controller type to Pro-Controller, and set the npadInterfaceType.
        m_controllerDevice.deviceType = m_controllerInitializedType;
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
            Logger::instance().log("initController() hiddbgAttachHdlsWorkBuffer() failed.", std::to_string(R_DESCRIPTION(rc)));
        }

        rc = hiddbgAttachHdlsVirtualDevice(&m_controllerHandle, &m_controllerDevice);
        if (R_FAILED(rc)) {
            Logger::instance().log("initController() hiddbgAttachHdlsVirtualDevice() failed.", std::to_string(R_DESCRIPTION(rc)));
        }

        //init a dummy keyboard state for assignment between keypresses
        m_dummyKeyboardState.keys[3] = 0x800000000000000UL; // Hackfix found by Red: an unused key press (KBD_MEDIA_CALC) is required to allow sequential same-key presses. bitfield[3]
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
            Logger::instance().log("detachController() hiddbgDetachHdlsVirtualDevice() failed.", std::to_string(R_DESCRIPTION(rc)));
        }

        rc = hiddbgReleaseHdlsWorkBuffer(m_sessionId);
        if (R_FAILED(rc)) {
            Logger::instance().log("detachController() hiddbgReleaseHdlsWorkBuffer() failed.", std::to_string(R_DESCRIPTION(rc)));
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
     * @param The button to click.
     */
    void Controller::click(const HidNpadButton& btn) {
        press(btn);
        svcSleepThread(m_buttonClickSleepTime * 1e+6L);
        release(btn);
    }

    /**
     * @brief Simulate a button press.
     * @param The button to press.
     */
    void Controller::press(const HidNpadButton& btn) {
        initController();
        m_hiddbgHdlsState.buttons |= btn;
        Result rc = hiddbgSetHdlsState(m_controllerHandle, &m_hiddbgHdlsState);
        if (R_FAILED(rc)) {
            Logger::instance().log("press() hiddbgSetHdlsState() failed.", std::to_string(R_DESCRIPTION(rc)));
        }
    }

    /**
     * @brief Simulate a button release.
     * @param The button to release.
     */
    void Controller::release(const HidNpadButton& btn) {
        initController();
        m_hiddbgHdlsState.buttons &= ~btn;
        Result rc = hiddbgSetHdlsState(m_controllerHandle, &m_hiddbgHdlsState);
        if (R_FAILED(rc)) {
            Logger::instance().log("release() hiddbgSetHdlsState() failed.", std::to_string(R_DESCRIPTION(rc)));
        }
    }

    /**
     * @brief Set the state of a joystick.
     * @param The joystick (left or right).
     * @param X value.
     * @param Y value.
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
            Logger::instance().log("setStickState() hiddbgSetHdlsState() failed.", std::to_string(R_DESCRIPTION(rc)));
        }
    }

    /**
     * @brief Simulate touch input.
     * @param Touch state array.
     * @param Number of sequential touches.
     * @param Hold time in microseconds.
     * @param Whether to hold the touch.
     */
    void Controller::touch(std::vector<HidTouchState>& state, u64 sequentialCount, u64 holdTime, bool hold) {
        initController();
        state[0].delta_time = holdTime; // only the first touch needs this for whatever reason
        for (u32 i = 0; i < sequentialCount; i++) {
            hiddbgSetTouchScreenAutoPilotState(&state[i], 1);
            svcSleepThread(holdTime);
            if (!hold) {
                hiddbgSetTouchScreenAutoPilotState(NULL, 0);
                svcSleepThread(m_pollRate * 1e+6L);
            }
        }

        // send finger release event
        if (hold) {
            hiddbgSetTouchScreenAutoPilotState(NULL, 0);
            svcSleepThread(m_pollRate * 1e+6L);
        }

        hiddbgUnsetTouchScreenAutoPilotState();
    }

    /**
     * @brief Simulate keyboard input.
     * @param Keyboard autopilot states.
     * @param Number of sequential key presses.
     */
    void Controller::key(const std::vector<HiddbgKeyboardAutoPilotState>& states, u64 sequentialCount) {
        initController();
        HiddbgKeyboardAutoPilotState tempState = { 0 };
        u32 i;
        for (i = 0; i < sequentialCount; i++) {
            std::memcpy(&tempState.keys, states[i].keys, sizeof(u64) * 4);
            tempState.modifiers = states[i].modifiers;
            hiddbgSetKeyboardAutoPilotState(&tempState);
            svcSleepThread(m_keyPressSleepTime * 1e+6L);

            if (i != (sequentialCount - 1)) {
                if (std::memcmp(states[i].keys, states[i + 1].keys, sizeof(u64) * 4) == 0 && states[i].modifiers == states[i + 1].modifiers) {
                    hiddbgSetKeyboardAutoPilotState(&m_dummyKeyboardState);
                    svcSleepThread(m_pollRate * 1e+6L);
                }
            } else {
                hiddbgSetKeyboardAutoPilotState(&m_dummyKeyboardState);
                svcSleepThread(m_pollRate * 1e+6L);
            }
        }

        hiddbgUnsetKeyboardAutoPilotState();
    }

    /**
     * @brief Set the controller type from parameters.
     * @param Parameters vector.
     */
    void Controller::setControllerType(const std::vector<std::string>& params) {
        if (params.size() < 2) {
            Logger::instance().log("setControllerType() params size is less than 2.");
            return;
        }

        detachController();
        m_controllerInitializedType = (HidDeviceType)Utils::parseStringToInt(params[1]);
    }

    /**
     * @brief Start the PA controller thread for processing commands.
     * @param Queue for sending data.
     * @param Condition variable for the sender queue.
     */
    void Controller::startControllerThread(LockFreeQueue<std::vector<char>>& senderQueue, std::condition_variable& senderCv, std::atomic_bool& error) {
        if (m_ccThreadRunning) {
            Logger::instance().log("Controller thread already running.");
            return;
        }

        Logger::instance().log("Starting commandLoopPA thread.");
        m_ccThread = std::thread(&Controller::commandLoopPA, this, std::ref(senderQueue), std::ref(senderCv), std::ref(error));
    }

    /**
     * @brief Main loop for processing PA controller commands in a thread.
     * @param Queue for sending data.
     * @param Condition variable for the sender queue.
     */
    void Controller::commandLoopPA(LockFreeQueue<std::vector<char>>& senderQueue, std::condition_variable& senderCv, std::atomic_bool& error) {
        const std::chrono::microseconds earlyWake(1000);
        m_nextStateChange = WallClock::max();
        m_ccThreadRunning = true;
        Logger::instance().log("commandLoopPA() started.");

        std::unique_lock<std::mutex> lock(m_ccMutex);
        while (!error) {
            WallClock now = std::chrono::steady_clock::now();
            ControllerCommand cmd;
            if (now >= m_nextStateChange) {
                if (!m_ccQueue.empty()) {
                    m_ccQueue.pop(cmd);
                    Logger::instance().log("commandLoopPA() processing command (seqnum " + std::to_string(cmd.seqnum) + ").");
                    cqControllerState(cmd);
                    m_nextStateChange = now + std::chrono::milliseconds(cmd.milliseconds);
                } else {
                    Logger::instance().log("commandLoopPA() clearing state (seqnum " + std::to_string(cmd.seqnum) + ").");
                    cmd.state.clear();
                    cqControllerState(cmd);
                    m_nextStateChange = WallClock::max();
                }
            }

            //  We are done processing the state change, we are off the critical path.
            //  Now is the best time to send the finished messaged for the previous command.
            if (m_ccCurrentCommand.seqnum != 0){
                Logger::instance().log("cqSendState() command finished with seqnum: " + std::to_string(m_ccCurrentCommand.seqnum));
                std::string res = "cqCommandFinished " + std::to_string(m_ccCurrentCommand.seqnum) + "\r\n";
                senderQueue.push(std::vector<char>(res.begin(), res.end()));
                senderCv.notify_one();
            }
            m_ccCurrentCommand = cmd;

            m_ccCv.wait_until(lock, m_nextStateChange - earlyWake, [&] { return error || now + earlyWake >= m_nextStateChange; });
        }

        m_ccQueue.clear();
        cqControllerState(ControllerCommand{});
        detachController();
        m_ccThreadRunning = false;
        error.store(true);
        Logger::instance().log("commandLoopPA() stopped thread.");
    }

    /**
     * @brief Update the PA controller state and optionally send a response.
     * @param The PA controller command.
     */
    void Controller::cqControllerState(const ControllerCommand& cmd) {
        Logger::instance().log("cqControllerState() called with seqnum: " + std::to_string(cmd.seqnum));
        initController();

        m_hiddbgHdlsState.buttons = cmd.state.buttons;
        m_hiddbgHdlsState.analog_stick_l.x = cmd.state.left_joystick_x;
        m_hiddbgHdlsState.analog_stick_l.y = cmd.state.left_joystick_y;
        m_hiddbgHdlsState.analog_stick_r.x = cmd.state.right_joystick_x;
        m_hiddbgHdlsState.analog_stick_r.y = cmd.state.right_joystick_y;

        Result rc = hiddbgSetHdlsState(m_controllerHandle, &m_hiddbgHdlsState);
        if (R_FAILED(rc)) {
            Logger::instance().log("cqControllerState() hiddbgSetHdlsState() failed.", std::to_string(R_DESCRIPTION(rc)));
        }
    }

    /**
     * @brief Enqueue a PA controller command for processing.
     * @param The controller command.
     */
    void Controller::cqEnqueueCommand(const ControllerCommand& cmd) {
        std::lock_guard<std::mutex> lock(m_ccMutex);
        Logger::instance().log("cqEnqueueCommand() pushing command with seqnum: " + std::to_string(cmd.seqnum));

        if (m_replaceOnNext) {
            m_replaceOnNext = false;
            m_ccCurrentCommand = ControllerCommand{};
            m_ccQueue.clear();
            m_nextStateChange = WallClock::min();
            m_ccQueue.push(cmd);
            m_ccCv.notify_all();
            return;
        }

        if (m_nextStateChange == WallClock::max()) {
            m_nextStateChange = WallClock::min();
            m_ccCv.notify_all();
        }

        m_ccQueue.push(cmd);
        m_ccCv.notify_all();
    }

    /**
     * @brief Cancel all queued PA controller commands.
     */
    void Controller::cqCancel() {
        std::lock_guard<std::mutex> lock(m_ccMutex);
        Logger::instance().log("cqCancel().");
        m_ccCurrentCommand = ControllerCommand{};
        m_ccQueue.clear();
        m_nextStateChange = WallClock::min();
        m_ccCv.notify_all();
    }

    /**
     * @brief Replace the next PA controller command on the next enqueue.
     */
    void Controller::cqReplaceOnNext() {
        Logger::instance().log("cqReplaceOnNext().");
        m_replaceOnNext = true;
    }

    /**
     * @brief Notify all PA threads.
     */
    void Controller::cqNotifyAll() {
        m_ccCv.notify_all();
    }

    /**
     * @brief Parse a string to a button value.
     * @param The string argument.
     * @return The button value, or -1 if not found.
     */
    int Controller::parseStringToButton(const std::string& arg) {
        auto it = Controller::m_button.find(arg);
        if (it != Controller::m_button.end()) {
            return it->second;
        } else {
            Logger::instance().log("parseStringToButton() button not found (" + arg + ").");
            return -1;
        }
    }

    /**
     * @brief Parse a string to a stick value.
     * @param The string argument.
     * @return The stick value, or -1 if not found.
     */
    int Controller::parseStringToStick(const std::string& arg) {
        auto it = Controller::m_stick.find(arg);
        if (it != Controller::m_stick.end()) {
            return it->second;
        } else {
            Logger::instance().log("parseStringToStick() stick not found (" + arg + ").");
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
