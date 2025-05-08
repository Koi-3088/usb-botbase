#pragma once

#include "moduleBase.h"
#include <switch.h>
#include <unordered_map>

namespace ControllerCommands {
	class Controller : protected virtual ModuleBase::BaseCommands {
	public:
		Controller() : BaseCommands() {
			m_workMem = (u8*)aligned_alloc(0x1000, m_workMem_size);
			m_controllerHandle = { 0 };
			m_controllerDevice = { 0 };
			m_controllerState = { 0 };
			m_sessionId = { 0 };
			m_dummyKeyboardState = { 0 };
			m_ControllerIsInitialised = false;
			m_controllerInitializedType = HidDeviceType_FullKey3;
		};

		~Controller() override {
			free(m_workMem);
			m_workMem = nullptr;
		};

		static int parseStringToButton(const std::string& arg);
		static int parseStringToStick(const std::string& arg);

	protected:
		void initController();
		void detachController();

		void click(HidNpadButton btn);
		void press(HidNpadButton btn);
		void release(HidNpadButton btn);
		void setStickState(Joystick stick, int dxVal, int dyVal);
		void touch(std::vector<HidTouchState>& state, u64 sequentialCount, u64 holdTime, bool hold);
		void key(const std::vector<HiddbgKeyboardAutoPilotState>& states, u64 sequentialCount);
		void setControllerType(const std::vector<std::string>& params);

	private:
		bool m_ControllerIsInitialised;
		HidDeviceType m_controllerInitializedType;

		HiddbgHdlsHandle m_controllerHandle;
		HiddbgHdlsDeviceInfo m_controllerDevice;
		HiddbgHdlsState m_controllerState;
		HiddbgKeyboardAutoPilotState m_dummyKeyboardState;
		HiddbgHdlsSessionId m_sessionId;

		u8* m_workMem = nullptr;
		const size_t m_workMem_size = 0x1000;

		struct TouchData {
			HidTouchState* states;
			u64 sequentialCount;
			u64 holdTime;
			bool hold;
			u8 state;
		};

		struct KeyData {
			HiddbgKeyboardAutoPilotState* states;
			u64 sequentialCount;
			u8 state;
		};

		static std::unordered_map<std::string, int> m_button;
		static std::unordered_map<std::string, int> m_stick;
    };
}
