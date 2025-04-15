#pragma once

#include "defines.h"
#include "moduleBase.h"
#include <switch.h>
#include <vector>
#include <unordered_map>

// Change this to enum later on.
#define JOYSTICK_LEFT 0
#define JOYSTICK_RIGHT 1

namespace ControllerCommands {
	class Controller : protected ModuleBase::BaseCommands {
	public:
		Controller() : BaseCommands() {
			m_workMem = (u8*)aligned_alloc(0x1000, m_workMem_size);
			controllerHandle = { 0 };
			controllerDevice = { 0 };
			controllerState = { 0 };
			sessionId = { 0 };
			dummyKeyboardState = { 0 };
			bControllerIsInitialised = false;
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
		void setStickState(int side, int dxVal, int dyVal);
		void touch(HidTouchState* state, u64 sequentialCount, u64 holdTime, bool hold, u8* token);
		void key(HiddbgKeyboardAutoPilotState* states, u64 sequentialCount);
		void clickSequence(char* seq, u8* token);

	private:
		bool bControllerIsInitialised;
		HiddbgHdlsHandle controllerHandle;
		HiddbgHdlsDeviceInfo controllerDevice;
		HiddbgHdlsState controllerState;
		HiddbgKeyboardAutoPilotState dummyKeyboardState;
		HiddbgHdlsSessionId sessionId;

		const u64 buttonClickSleepTime = 50;
		const u64 keyPressSleepTime = 25;
		const u64 pollRate = 17;
		const u32 fingerDiameter = 50;

		u8* m_workMem = nullptr;
		const size_t m_workMem_size = 0x1000;

		typedef struct {
			HidTouchState* states;
			u64 sequentialCount;
			u64 holdTime;
			bool hold;
			u8 state;
		} TouchData;

		typedef struct {
			HiddbgKeyboardAutoPilotState* states;
			u64 sequentialCount;
			u8 state;
		} KeyData;

		static std::unordered_map<std::string, int> m_button;
    };
}
