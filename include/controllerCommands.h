#pragma once

#include "moduleBase.h"
#include <malloc.h>
#include <switch.h>
#include <unordered_map>
#include <chrono>
#include <mutex>

namespace ControllerCommands {
	class Controller : protected virtual ModuleBase::BaseCommands {
	public:
        Controller() : BaseCommands() {
           m_workMem = (u8*)aligned_alloc(0x1000, m_workMem_size);
           m_controllerHandle = { 0 };
           m_controllerDevice = { 0 };
		   m_hiddbgHdlsState = { 0 };
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

	public:
		using WallClock = std::chrono::steady_clock::time_point;
		struct ControllerState {
			uint64_t buttons = 0;
			int16_t left_joystick_x = 0;
			int16_t left_joystick_y = 0;
			int16_t right_joystick_x = 0;
			int16_t right_joystick_y = 0;

			bool isNeutral() const {
				return buttons == 0
					&& left_joystick_x == 0
					&& left_joystick_y == 0
					&& right_joystick_x == 0
					&& right_joystick_y == 0;
			}

			void clear() {
				buttons &= ~0;
				left_joystick_x = 0;
				left_joystick_y = 0;
				right_joystick_x = 0;
				right_joystick_y = 0;
			}
		};

		struct ControllerCommand {
			uint64_t seqnum;
			uint64_t milliseconds;
			ControllerState state {};

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
					hi = (hi >= '0' && hi <= '9') ? hi - '0' :
						(hi >= 'a' && hi <= 'f') ? hi - 'a' + 10 :
						(hi >= 'A' && hi <= 'F') ? hi - 'A' + 10 : 0;
					lo = (lo >= '0' && lo <= '9') ? lo - '0' :
						(lo >= 'a' && lo <= 'f') ? lo - 'a' + 10 :
						(lo >= 'A' && lo <= 'F') ? lo - 'A' + 10 : 0;
					ptr[0] = hi << 4 | lo;
					ptr++;
				}
			}
		};

		static std::mutex m_stateMutex;
		static ControllerCommand m_controllerCommand;
		static std::atomic_bool m_replaceOnNext;
		static WallClock m_nextStateChange;

	public:
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
		void cqControllerState(const ControllerCommand& cmd, std::vector<char>& buffer);
		void cqControllerStateClear(const ControllerCommand& cmd, std::vector<char>& buffer);

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
		std::atomic_bool m_controllerIsInitialised { false };
		HidDeviceType m_controllerInitializedType;

		HiddbgHdlsHandle m_controllerHandle;
		HiddbgHdlsDeviceInfo m_controllerDevice;
		HiddbgHdlsState m_hiddbgHdlsState;
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
