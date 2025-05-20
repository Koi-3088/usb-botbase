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
		struct Command {
			uint64_t seqnum;
			uint64_t milliseconds;
			uint64_t buttons;
			uint16_t left_joystick_x;
			uint16_t left_joystick_y;
			uint16_t right_joystick_x;
			uint16_t right_joystick_y;

			void writeToHex(char str[64]) const {
				const char HEX_DIGITS[] = "0123456789abcdef";
				const char* ptr = (const char*)this;
				for (size_t c = 0; c < 64; c += 2) {
					uint8_t hi = (uint8_t)ptr[0] >> 4;
					uint8_t lo = (uint8_t)ptr[0] & 0x0f;
					str[c + 0] = HEX_DIGITS[hi];
					str[c + 1] = HEX_DIGITS[lo];
					ptr++;
				}
			}

			void parseFromHex(const char str[64]) {
				char* ptr = (char*)this;
				for (size_t c = 0; c < 64; c += 2) {
					char hi = str[c + 0];
					char lo = str[c + 1];
					hi = hi < 'a' ? hi - '0' : hi - 'a' + 10;
					lo = lo < 'a' ? lo - '0' : lo - 'a' + 10;
					ptr[0] = hi << 4 | lo;
					ptr++;
				}
			}
		};

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
		void handleCcCommand(const Command& cmd, std::vector<char>& buffer);

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
