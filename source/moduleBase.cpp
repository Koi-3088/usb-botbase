#include "defines.h"
#include "moduleBase.h"
#include "ntp.h"
#include <ctime>
#include "logger.h"

namespace ModuleBase {
	using namespace Util;
    using namespace SbbLog;
	using namespace NTP;

    /**
     * @brief Attach to the current application process for debugging.
     * @return true if attach succeeded, false otherwise.
     */
    bool BaseCommands::attach() {
        u64 pid = 0;
        Result rc = pmdmntGetApplicationProcessId(&pid);
        if (R_FAILED(rc)) {
            Logger::instance().log("attach() pmdmntGetApplicationProcessId() failed.", std::to_string(R_DESCRIPTION(rc)));
            return false;
        }

        if (m_metaData.pid != pid) {
            Logger::instance().log("attach() m_metaData.pid != pid, calling initMetaData().");
            m_metaData.pid = pid;
            initMetaData();
        } else {
            detach();
        }

        rc = svcDebugActiveProcess(&m_debugHandle, pid);
        if (R_FAILED(rc)) {
            Logger::instance().log("attach() svcDebugActiveProcess() failed.", std::to_string(R_DESCRIPTION(rc)));
            return false;
        }

        return true;
    }

    /**
     * @brief Detach from the debugged process.
     */
    void BaseCommands::detach() {
        if (m_debugHandle != 0) {
            svcCloseHandle(m_debugHandle);
        }
    }

    /**
     * @brief Initialize metadata for the current process.
     */
    void BaseCommands::initMetaData() {
        if (!attach()) {
            Logger::instance().log("initMetaData() attach() failed.");
            detach();
            return;
        }

        m_metaData.main_nso_base = getMainNsoBase();
        m_metaData.heap_base = getHeapBase();
        m_metaData.titleID = getTitleId();
        m_metaData.titleVersion = GetTitleVersion();
        m_metaData.buildID = getBuildID();

        detach();
        if (metaHasZeroValue(m_metaData)) {
            Logger::instance().log("initMetaData() MetaData had one or more zero value.");
        }
    }

    /**
     * @brief Get the build ID of the main module.
     * @return The build ID byte.
     */
    u8 BaseCommands::getBuildID() {
        LoaderModuleInfo proc_modules[2];
        s32 numModules = 0;
        Result rc = ldrDmntGetProcessModuleInfo(m_metaData.pid, proc_modules, 2, &numModules);
        if (R_FAILED(rc)) {
            Logger::instance().log("getBuildID() ldrDmntGetProcessModuleInfo() failed.", std::to_string(R_DESCRIPTION(rc)));
            return 0;
        }

        if (numModules == 2) {
            return proc_modules[1].build_id[0];
        } else {
            return proc_modules[0].build_id[0];
        }
    }

    /**
     * @brief Get the base address of the main NSO module.
     * @return The base address.
     */
    u64 BaseCommands::getMainNsoBase() {
        LoaderModuleInfo proc_modules[2];
        s32 numModules = 0;
        Result rc = ldrDmntGetProcessModuleInfo(m_metaData.pid, proc_modules, 2, &numModules);
        if (R_FAILED(rc)) {
            Logger::instance().log("getMainNsoBase() ldrDmntGetProcessModuleInfo() failed.", std::to_string(R_DESCRIPTION(rc)));
            return 0;
        }

        if (numModules == 2) {
            return proc_modules[1].base_address;
        } else {
            return proc_modules[0].base_address;
        }
    }

    /**
     * @brief Get the base address of the heap region.
     * @return The heap base address.
     */
    u64 BaseCommands::getHeapBase() {
        u64 heap_base = 0;
        Result rc = svcGetInfo(&heap_base, InfoType_HeapRegionAddress, m_debugHandle, 0);
        if (R_FAILED(rc)) {
            Logger::instance().log("getHeapBase() svcGetInfo() failed.", std::to_string(R_DESCRIPTION(rc)));
            return 0;
        }

        return heap_base;
    }

    /**
     * @brief Get the title ID of the current process.
     * @return The title ID.
     */
    u64 BaseCommands::getTitleId() {
        u64 titleId = 0;
        Result rc = pminfoGetProgramId(&titleId, m_metaData.pid);
        if (R_FAILED(rc)) {
            Logger::instance().log("getTitleId() pminfoGetProgramId() failed.", std::to_string(R_DESCRIPTION(rc)));
            return 0;
        }

        return titleId;
    }

    /**
     * @brief Get the title version of the current process.
     * @return The title version.
     */
    u64 BaseCommands::GetTitleVersion() {
        Result rc = nsInitialize();
        if (R_FAILED(rc)) {
            Logger::instance().log("GetTitleVersion() nsInitialize() failed.", std::to_string(R_DESCRIPTION(rc)));
            return 0;
        }

        u64 titleV = 0;
        s32 out = 0;
        std::vector<NsApplicationContentMetaStatus> metaStatus(100U);
        rc = nsListApplicationContentMetaStatus(m_metaData.titleID, 0, metaStatus.data(), sizeof(NsApplicationContentMetaStatus), &out);
        nsExit();
        if (R_FAILED(rc)) {
            Logger::instance().log("GetTitleVersion() nsListApplicationContentMetaStatus() failed.", std::to_string(R_DESCRIPTION(rc)));
            return 0;
        }

        for (int i = 0; i < out; i++) {
            if (titleV < metaStatus[i].version) {
                titleV = metaStatus[i].version;
            }
        }

        return (titleV / 0x10000);
    }

    /**
     * @brief Get application control data for the current title.
     * @param[out] out Output size.
     * @return Vector of NsApplicationControlData.
     */
    std::vector<NsApplicationControlData> BaseCommands::getNsApplicationControlData(u64& out) {
        Result rc = nsInitialize();
        if (R_FAILED(rc)) {
            Logger::instance().log("getNsApplicationControlData() nsInitialize() failed.", std::to_string(R_DESCRIPTION(rc)));
            return {};
        }

        std::vector<NsApplicationControlData> buf(1);
        rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, m_metaData.titleID, buf.data(), sizeof(NsApplicationControlData), &out);
        nsExit();
        if (R_FAILED(rc)) {
            Logger::instance().log("getNsApplicationControlData() nsGetApplicationControlData() failed.", std::to_string(R_DESCRIPTION(rc)));
            return {};
        }

        return buf;
    }

    /**
     * @brief Set the display power state.
     * @param state The desired power state.
     */
    void BaseCommands::setScreen(const ViPowerState& state) {
        ViDisplay temp_display;
        Result rc = viOpenDisplay("Internal", &temp_display);
        if (R_FAILED(rc)) {
            Logger::instance().log("setScreen() viOpenDisplay() failed.", std::to_string(R_DESCRIPTION(rc)));
            rc = viOpenDefaultDisplay(&temp_display);
        }

        if (R_SUCCEEDED(rc)) {
            rc = viSetDisplayPowerState(&temp_display, state);
            svcSleepThread(1e+6l);
            viCloseDisplay(&temp_display);

            rc = lblInitialize();
            if (R_FAILED(rc)) {
                Logger::instance().log("setScreen() lblInitialize() failed.", std::to_string(R_DESCRIPTION(rc)));
            }

            if (state == ViPowerState_On) {
                lblSwitchBacklightOn(1ul);
            } else {
                lblSwitchBacklightOff(1ul);
            }

            lblExit();
        }
    }

    /**
     * @brief Check if a program with the given ID is open.
     * @param id The program ID.
     * @return true if open, false otherwise.
     */
    bool BaseCommands::getIsProgramOpen(u64 id) {
        u64 pid = 0;
        Result rc = pmdmntGetProcessId(&pid, id);
        return !(pid == 0 || R_FAILED(rc));
    }

    /**
     * @brief Set the button click sleep time from parameters.
     * @param The parameters vector.
     */
    void BaseCommands::setButtonClickSleepTime(const std::vector<std::string>& params) {
        if (params.size() < 2) {
            Logger::instance().log("setKeySleepTime() params size is less than 2.");
            return;
        }

        m_buttonClickSleepTime = Utils::parseStringToInt(params[1]);
    }

    /**
     * @brief Set the key press sleep time from parameters.
     * @param The parameters vector.
     */
    void BaseCommands::setKeySleepTime(const std::vector<std::string>& params) {
        if (params.size() < 2) {
            Logger::instance().log("setKeySleepTime() params size is less than 2.");
            return;
        }

        m_keyPressSleepTime = Utils::parseStringToInt(params[1]);
    }

    /**
     * @brief Set the finger diameter from parameters.
     * @param The parameters vector.
     */
    void BaseCommands::setFingerDiameter(const std::vector<std::string>& params) {
        if (params.size() < 2) {
            Logger::instance().log("setFingerDiameter() params size is less than 2.");
            return;
        }

        m_fingerDiameter = Utils::parseStringToInt(params[1]);
    }

    /**
     * @brief Set the poll rate from parameters.
     * @param The parameters vector.
     */
    void BaseCommands::setPollRate(const std::vector<std::string>& params) {
        if (params.size() < 2) {
            Logger::instance().log("setPollRate() params size is less than 2.");
            return;
        }

        m_pollRate = Utils::parseStringToInt(params[1]);
    }

    /**
     * @brief Set whether PA is enabled from parameters.
     * @param The parameters vector.
     */
    void BaseCommands::setEnabledPA(const std::vector<std::string>& params) {
        if (params.size() < 2) {
            Logger::instance().log("setEnabledPA() params size is less than 2.");
            return;
        }

        m_isEnabledPA = (bool)Utils::parseStringToInt(params[1]);
    }

    /**
     * @brief Set whether logs are enabled from parameters.
     * @param The parameters vector.
     */
    void BaseCommands::setEnabledLogs(const std::vector<std::string>& params) {
        if (params.size() < 2) {
            Logger::instance().log("setEnabledLogs() params size is less than 2.");
            return;
        }

        bool enable = (bool)Utils::parseStringToInt(params[1]);
        Logger::instance().enableLogs(enable);
    }

    /**
     * @brief Set whether backwards compatibility is enabled from parameters.
     * @param The parameters vector.
     */
    void BaseCommands::setEnabledBackwards(const std::vector<std::string>& params) {
        if (params.size() < 2) {
            Logger::instance().log("setEnabledBackwards() params size is less than 2.");
            return;
        }

        bool enable = (bool)Utils::parseStringToInt(params[1]);
        g_enableBackwardsCompat = enable;
    }

    /**
     * @brief Get the game icon data.
     * @param[out] buffer Output buffer for icon data.
     */
    void BaseCommands::getGameIcon(std::vector<char>& buffer) {
        u64 out = 0;
        auto data = getNsApplicationControlData(out);
        if (data.empty()) {
            return;
        }

        out -= sizeof(data[0].nacp);
        buffer.resize(out);
        std::copy(reinterpret_cast<const char*>(&data[0].icon),
            reinterpret_cast<const char*>(&data[0].icon) + out,
            buffer.begin());
    }

    /**
     * @brief Get the game version string.
     * @param[out] buffer Output buffer for version string.
     */
    void BaseCommands::getGameVersion(std::vector<char>& buffer) {
        u64 out = 0;
        auto data = getNsApplicationControlData(out);
        if (data.empty()) {
            return;
        }

        buffer.resize(sizeof(data[0].nacp.display_version));
        std::copy(reinterpret_cast<const char*>(&data[0].nacp.display_version[0]),
            reinterpret_cast<const char*>(&data[0].nacp.display_version[15]),
            buffer.begin());

        buffer.erase(std::remove(buffer.begin(), buffer.end(), '\0'), buffer.end());
    }

    /**
     * @brief Get the game rating.
     * @param[out] buffer Output buffer for rating.
     */
    void BaseCommands::getGameRating(std::vector<char>& buffer) {
        u64 out = 0;
        auto data = getNsApplicationControlData(out);
        if (data.empty()) {
            return;
        }

        buffer.resize(sizeof(int));
        std::copy(reinterpret_cast<const char*>(&data[0].nacp.rating_age[0]),
            reinterpret_cast<const char*>(&data[0].nacp.rating_age[0] + sizeof(int)),
            buffer.begin());
    }

    /**
     * @brief Get the game author string.
     * @param[out] buffer Output buffer for author string.
     */
    void BaseCommands::getGameAuthor(std::vector<char>& buffer) {
        u64 out = 0;
        auto data = getNsApplicationControlData(out);
        if (data.empty()) {
            return;
        }

        NacpLanguageEntry* lang = nullptr;
        Result rc = nacpGetLanguageEntry(&data[0].nacp, &lang);
        if (R_FAILED(rc)) {
            Logger::instance().log("getGameAuthor() nacpGetLanguageEntry() failed.", std::to_string(R_DESCRIPTION(rc)));
            delete lang;
            return;
        }

        buffer.resize(out);
        std::copy(reinterpret_cast<const char*>(&lang->author[0]),
            reinterpret_cast<const char*>(&lang->author[255]),
            buffer.begin());

        buffer.erase(std::remove(buffer.begin(), buffer.end(), '\0'), buffer.end());
        delete lang;
    }

    /**
     * @brief Get the game name string.
     * @param[out] buffer Output buffer for name string.
     */
    void BaseCommands::getGameName(std::vector<char>& buffer) {
        u64 out = 0;
        auto data = getNsApplicationControlData(out);
        if (data.empty()) {
            return;
        }

        NacpLanguageEntry* lang = nullptr;
        Result rc = nacpGetLanguageEntry(&data[0].nacp, &lang);
        if (R_FAILED(rc)) {
            Logger::instance().log("getGameName() nacpGetLanguageEntry() failed.", std::to_string(R_DESCRIPTION(rc)));
            delete lang;
            return;
        }

        buffer.resize(out);
        std::copy(reinterpret_cast<const char*>(&lang->name[0]),
            reinterpret_cast<const char*>(&lang->name[511]),
            buffer.begin());

        buffer.erase(std::remove(buffer.begin(), buffer.end(), '\0'), buffer.end());
        delete lang;
    }

    /**
     * @brief Get the current Switch time.
     * @param[out] buffer Output buffer for time value.
     */
    void BaseCommands::getSwitchTime(std::vector<char>& buffer) {
        time_t posix = 0;
        buffer.resize(sizeof(posix));

        Result rc = timeGetCurrentTime(TimeType_UserSystemClock, (u64*)&posix);
        if (R_SUCCEEDED(rc)) {
            std::tm* time = localtime(&posix);
            if (time->tm_year >= 160 || time->tm_year < 100) { // >= 2060 || < 2000
                Logger::instance().log("getSwitchTime() invalid time range, setting time to 2000-01-01.");
                time->tm_year = 100;
                time->tm_mon = 0;
                time->tm_mday = 1;

                rc = timeSetCurrentTime(TimeType_NetworkSystemClock, mktime(time));
                if (R_SUCCEEDED(rc)) {
                    Logger::instance().log("getSwitchTime() timeSetCurrentTime() succeeded, set time to 2000-01-01.");
                    posix = mktime(time);
                } else {
                    Logger::instance().log("getSwitchTime() timeSetCurrentTime() failed.", std::to_string(R_DESCRIPTION(rc)));
                    posix = 0;
                }
            } else {
                posix = mktime(time);
            }
        } else {
            Logger::instance().log("getSwitchTime() timeGetCurrentTime(TimeType_UserSystemClock) failed.", std::to_string(R_DESCRIPTION(rc)));
            posix = 0;
        }

        std::copy(reinterpret_cast<const char*>(&posix),
            reinterpret_cast<const char*>(&posix) + sizeof(time_t),
            buffer.begin());
    }

    /**
     * @brief Set the Switch time.
     * @param Parameters containing the time value.
     * @param[out] Output buffer indicating success.
     */
    void BaseCommands::setSwitchTime(const std::vector<std::string>& params, std::vector<char>& buffer) {
        bool success = false;
        buffer.resize(sizeof(success));

        time_t input = (time_t)std::stoull(params[0], NULL, 10);
        std::tm* toSet = localtime(&input);
        if (toSet->tm_year >= 100 || toSet->tm_year <= 160) { // >= 2000 || <= 2060
            Result rc = timeSetCurrentTime(TimeType_NetworkSystemClock, input);
            if (R_SUCCEEDED(rc)) {
                success = true;
            } else {
                Logger::instance().log("setSwitchTime() timeSetCurrentTime() failed.", std::to_string(R_DESCRIPTION(rc)));
            }
        } else {
            Logger::instance().log("setSwitchTime() invalid time range.");
        }

        std::copy(reinterpret_cast<const char*>(&success),
            reinterpret_cast<const char*>(&success) + sizeof(bool),
            buffer.begin());
    }

    /**
     * @brief Reset the Switch time using NTP if available.
     * @param[out] buffer Output buffer indicating success.
     */
    void BaseCommands::resetSwitchTime(std::vector<char>& buffer) {
        bool success = false;
        buffer.resize(sizeof(success));

        Result rc = setsysInitialize();
        if (R_SUCCEEDED(rc)) {
            bool sync;
            rc = setsysIsUserSystemClockAutomaticCorrectionEnabled(&sync);
            setsysExit();

            if (R_SUCCEEDED(rc) && isConnectedToInternet()) {
                NTPClient ntpClient;
                time_t ntp = ntpClient.getTime();
                if (ntp != 0) {
                    rc = timeSetCurrentTime(TimeType_NetworkSystemClock, ntp);
                    if (R_SUCCEEDED(rc)) {
                        success = true;
                    } else {
                        Logger::instance().log("resetSwitchTime() failed to set the network clock.", std::to_string(R_DESCRIPTION(rc)));
                    }
                }
            } else {
                Logger::instance().log("resetSwitchTime() failed to check if internet time sync is enabled.", std::to_string(R_DESCRIPTION(rc)));
            }
        } else {
            Logger::instance().log("resetSwitchTime() setsysInitialize() failed.", std::to_string(R_DESCRIPTION(rc)));
        }

        std::copy(reinterpret_cast<const char*>(&success),
            reinterpret_cast<const char*>(&success) + sizeof(bool),
            buffer.begin());
    }

    /**
     * @brief Check if the system is connected to the internet.
     * @return true if connected, false otherwise.
     */
    bool BaseCommands::isConnectedToInternet() {
        Result rc = nifmInitialize(NifmServiceType_User);
        if (R_FAILED(rc)) {
            Logger::instance().log("isConnectedToInternet() nifmInitialize() failed.", std::to_string(R_DESCRIPTION(rc)));
            return false;
        }

        NifmInternetConnectionStatus status;
        rc = nifmGetInternetConnectionStatus(NULL, NULL, &status);
        nifmExit();
        if (R_FAILED(rc) || status != NifmInternetConnectionStatus_Connected) {
            Logger::instance().log("isConnectedToInternet() nifmGetInternetConnectionStatus() failed or not connected.", std::to_string(R_DESCRIPTION(rc)));
            return false;
        }

        return true;
    }

    /**
     * @brief Check if any value in MetaData is zero.
     * @param m The MetaData struct.
     * @return true if any value is zero, false otherwise.
     */
    bool BaseCommands::metaHasZeroValue(const MetaData& m) {
        return (m.buildID == 0 || m.heap_base == 0 || m.main_nso_base == 0
            || m.pid == 0 || m.titleID == 0 || m.titleVersion == 0);
    }
}
