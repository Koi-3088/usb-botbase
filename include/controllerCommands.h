#pragma once

#include "moduleBase.h"
#include <malloc.h>
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
           m_controllerIsInitialised = false;
		   m_controllerDevice.npadInterfaceType = HidNpadInterfaceType_USB;
           m_controllerInitializedType = HidDeviceType_FullKey3;
        };

		~Controller() override {
			detachController();
			aligned_free(m_workMem);
			m_workMem = nullptr;
			m_controllerIsInitialised = false;
		};

		static int parseStringToButton(const std::string& arg);
		static int parseStringToStick(const std::string& arg);

	protected:
		void initController();
		void detachController();

		void click(const HidNpadButton& btn);
		void press(const HidNpadButton& btn);
		void release(const HidNpadButton& btn);
		void setStickState(const Joystick& stick, int dxVal, int dyVal);
		void touch(std::vector<HidTouchState>& state, u64 sequentialCount, u64 holdTime, bool hold);
		void key(const std::vector<HiddbgKeyboardAutoPilotState>& states, u64 sequentialCount);
		void setControllerType(const std::vector<std::string>& params);

	private:
		inline void* aligned_alloc(size_t alignment, size_t size) {
			if (alignment < sizeof(void*) || (alignment & (alignment - 1)) != 0) {
				return nullptr;
			}

			void* original = malloc(size + alignment - 1 + sizeof(void*));
			if (!original) {
				return nullptr;
			}

			uintptr_t raw = reinterpret_cast<uintptr_t>(original) + sizeof(void*);
			uintptr_t aligned = (raw + alignment - 1) & ~(alignment - 1);
			reinterpret_cast<void**>(aligned)[-1] = original;
			return reinterpret_cast<void*>(aligned);
		}

		inline void aligned_free(void* ptr) {
			if (ptr) {
				free(reinterpret_cast<void**>(ptr)[-1]);
			}
		}

	private:
		bool m_controllerIsInitialised;
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
