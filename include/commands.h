#pragma once

#include "defines.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <switch.h>
#include "connection.h"

namespace Commands {
	using namespace Connection;
	class CommandHandler {
	public:
		CommandHandler() {}

		~CommandHandler() {
			//delete m_connection;
			//m_connection = nullptr;
			//m_connection.reset();
		}

		typedef struct {
			u64 main_nso_base;
			u64 heap_base;
			u64 titleID;
			u64 titleVersion;
			u8 buildID[0x20];
		} MetaData;

		std::vector<char> HandleCommand(const std::string& cmd, const std::vector<std::string>& params, int sockfd = 0);

	private:
		//ConnectionHandler* m_connection;
		Handle debughandle = 0;
		const std::string m_sbbVersion = "2.4.1\n";

		void attach();
		void detach();
		MetaData getMetaData();
		void getBuildID(MetaData* meta, u64 pid);
		u64 getMainNsoBase(u64 pid);
		u64 getHeapBase(Handle handle);
		u64 getTitleId(u64 pid);
		u64 GetTitleVersion(u64 pid, u64 titleID);
		u64 getoutsize(NsApplicationControlData* buf);
		void peekInfinite(u64 offset, u64 size);
		void readMem(u64* out, u64 offset, u64 size);

		enum class CommandEnum {
			Peek,
			PeekMulti,
			PeekAbsolute,
			PeekAbsoluteMulti,
			PeekMain,
			PeekMainMulti,
			Poke,
			PokeAbsolute,
			PokeMain,

			Pointer,
			PointerAll,
			PointerRelative,
			PointerPeek,
			PointerPeekMulti,
			PointerPoke,

			Click,
			ClickSeq,
			ClickCancel,
			Press,
			Release,
			SetStick,
			DetachController,

			Touch,
			TouchHold,
			TouchDraw,
			TouchCancel,

			Key,
			KeyMod,
			KeyMulti,

			Game,
			Configure,
			GetTitleID,
			GetTitleVersion,
			GetSystemLanguage,
			GetMainNsoBase,
			GetBuildID,
			GetHeapBase,
			IsProgramRunning,
			PixelPeek,
			GetVersion,
			ScreenOn,
			ScreenOff,
			Charge,
			FdCount,

			Freeze,
			UnFreeze,
			FreezeCount,
			FreezeClear,
			FreezePause,
			FreezeUnpause,

			GetSwitchTime,
			SetSwitchTime,
			ResetSwitchTime,
		};

		std::unordered_map<std::string, CommandEnum> m_cmd {
			{ "peek", CommandEnum::Peek },
			{ "peekMulti", CommandEnum::PeekMulti },
			{ "peekAbsolute", CommandEnum::PeekAbsolute },
			{ "peekAbsoluteMulti", CommandEnum::PeekAbsoluteMulti },
			{ "peekMain", CommandEnum::PeekMain },
			{ "peekMainMulti", CommandEnum::PeekMainMulti },
			{ "poke", CommandEnum::Poke },
			{ "pokeAbsolute", CommandEnum::PokeAbsolute },
			{ "pokeMain", CommandEnum::PokeMain },

			{ "pointer", CommandEnum::Pointer },
			{ "pointerAll", CommandEnum::PointerAll },
			{ "pointerRelative", CommandEnum::PointerRelative },
			{ "pointerPeek", CommandEnum::PointerPeek},
			{ "pointerPeekMulti", CommandEnum::PointerPeekMulti },
			{ "pointerPoke", CommandEnum::PointerPoke },

			{ "click", CommandEnum::Click },
			{ "clickSeq", CommandEnum::ClickSeq },
			{ "clickCancel", CommandEnum::ClickCancel },
			{ "press", CommandEnum::Press },
			{ "release", CommandEnum::Release },
			{ "setStick", CommandEnum::SetStick },
			{ "detachController", CommandEnum::DetachController },

			{ "touch", CommandEnum::Touch },
			{ "touchHold", CommandEnum::TouchHold },
			{ "touchDraw", CommandEnum::TouchDraw },
			{ "touchCancel", CommandEnum::TouchCancel },

			{ "key", CommandEnum::Key },
			{ "keyMod", CommandEnum::KeyMod },
			{ "keyMulti", CommandEnum::KeyMulti },

			{ "game", CommandEnum::Game },
			{ "configure", CommandEnum::Configure },
			{ "getTitleID", CommandEnum::GetTitleID },
			{ "getTitleVersion", CommandEnum::GetTitleVersion },
			{ "getSystemLanguage", CommandEnum::GetSystemLanguage },
			{ "getMainNsoBase", CommandEnum::GetMainNsoBase },
			{ "getBuildID", CommandEnum::GetBuildID },
			{ "isProgramRunning", CommandEnum::IsProgramRunning },
			{ "pixelPeek", CommandEnum::PixelPeek },
			{ "getVersion", CommandEnum::GetVersion },
			{ "screenOn", CommandEnum::ScreenOn },
			{ "screenOff", CommandEnum::ScreenOff },
			{ "charge", CommandEnum::Charge },
			{ "fdCount", CommandEnum::FdCount },

			{ "freeze", CommandEnum::Freeze },
			{ "unFreeze", CommandEnum::UnFreeze },
			{ "freezeCount", CommandEnum::FreezeCount },
			{ "freezeClear", CommandEnum::FreezeClear },
			{ "freezePause", CommandEnum::FreezePause },
			{ "freezeUnpause", CommandEnum::FreezeUnpause },

			{ "getSwitchTime", CommandEnum::GetSwitchTime },
			{ "setSwitchTime", CommandEnum::SetSwitchTime },
			{ "resetSwitchTime", CommandEnum::ResetSwitchTime },
		};
	};
}
