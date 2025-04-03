#include "defines.h"
#include "usbConnection.h"
#include "log.h"

namespace UsbConnection {
    using namespace Util;
    using namespace SbbLog;

    Result UsbConnection::initialize(Result& res) {
        res = usbCommsInitialize();
        return res;
    }

	void UsbConnection::connect() {
        Utils::flashLed();

        while (appletMainLoop()) {
            auto buffer = UsbConnection::receive_data();
            Utils::parseArgs(buffer, [this](std::string x, const std::vector<std::string>& y) { return UsbConnection::argmain(x, y); });
            svcSleepThread(1e+6L);
        }
	}

    int UsbConnection::argmain(std::string cmd, const std::vector<std::string>& params, int sockfd) {
        if (cmd == "")
            return 0;

        std::string msg = "cmd: " + cmd;
        Logger::logToFile(msg);

        msg = "params #: " + std::to_string(params.size());
        Logger::logToFile(msg);

        auto it = map.find(cmd);
        if (it != map.end()) {
            std::string ver = "2.4.1\n";
            std::vector<char> buffer(ver.begin(), ver.end());

            msg = "cmd buffer: " + std::string(buffer.data());
            Logger::logToFile(msg);

            UsbConnection::sendData(buffer, buffer.size());
        }

        return 0;
    }

	extern "C" void UsbConnection::disconnect() {
		usbCommsExit();
	}

    extern "C" std::vector<char> UsbConnection::receive_data(int sockfd) {
        size_t size = 0;
        std::string msg = "Size before read: " + std::to_string(size);
        Logger::logToFile(msg);

        usbCommsRead(&size, sizeof(size));

        msg = "Size after initial read: " + std::to_string(size);
        Logger::logToFile(msg);

        auto buffer = std::vector<char>(size + 1);
        usbCommsRead((void*)buffer.data(), size);

        buffer[size - 1] = '\n';
        buffer[size - 2] = '\r';

        msg = "Read buffer: " + std::string(buffer.data());
        Logger::logToFile(msg);

        fflush(stdout);
        return buffer;
    }

    extern "C" void UsbConnection::sendData(const std::vector<char>& data, size_t data_size, int sockfd) {
        USBResponse response {
            data_size,
            (void*)data.data(),
        };

		usbCommsWrite(&response, 4);
		if (response.size > 0) {
            usbCommsWrite(response.data, response.size);
		}
    }
}
