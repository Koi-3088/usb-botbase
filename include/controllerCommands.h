#pragma once

#include "defines.h"
#include "moduleBase.h"
#include <switch.h>
#include <unordered_map>

namespace ControllerCommands {
	class Controller : protected virtual ModuleBase::BaseCommands {
	public:
		Controller() : BaseCommands() {
			m_workMem = (u8*)aligned_alloc(0x1000, m_workMem_size);
			controllerHandle = { 0 };
			controllerDevice = { 0 };
			controllerState = { 0 };
			sessionId = { 0 };
			dummyKeyboardState = { 0 };
			bControllerIsInitialised = false;
			controllerInitializedType = HidDeviceType_FullKey3;
		};

		~Controller() override {
			free(m_workMem);
			m_workMem = nullptr;
		};

		static int parseStringToButton(const std::string& arg);

	protected:
		void initController();
		void detachController();

		void click(HidNpadButton btn);
		void press(HidNpadButton btn);
		void release(HidNpadButton btn);
		void setStickState(Joystick stick, int dxVal, int dyVal);
		void touch(HidTouchState* state, u64 sequentialCount, u64 holdTime, bool hold, u8* token);
		void key(HiddbgKeyboardAutoPilotState* states, u64 sequentialCount);
		void clickSequence(char* seq, u8* token);

		HidDeviceType controllerInitializedType;

	private:
		bool bControllerIsInitialised;
		HiddbgHdlsHandle controllerHandle;
		HiddbgHdlsDeviceInfo controllerDevice;
		HiddbgHdlsState controllerState;
		HiddbgKeyboardAutoPilotState dummyKeyboardState;
		HiddbgHdlsSessionId sessionId;

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
    };
}
