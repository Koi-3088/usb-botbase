#include "defines.h"
#include "logger.h"
#include "usbConnection.h"
#include "util.h"
#include "commands.h"

namespace UsbConnection {
    using namespace Util;
    using namespace SbbLog;
    using namespace Commands;

    Result UsbConnection::initialize(Result& res) {
        res = usbCommsInitialize();
        return res;
    }

	void UsbConnection::connect() {
        Utils::flashLed();

        while (appletMainLoop()) {
            auto buffer = UsbConnection::receive_data();
            Utils::parseArgs(buffer, [&](std::string x, const std::vector<std::string>& y) {
                CommandHandler command;
                auto buffer = command.HandleCommand(x, y);
                if (buffer.empty()) {
                    return;
                }

                Logger::logToFile("Sending data...");
                UsbConnection::sendData(buffer, buffer.size());
            });
            svcSleepThread(1e+6L);
        }
	}

    /*int UsbConnection::argmain(std::string cmd, const std::vector<std::string>& params, int sockfd) {
        if (cmd.empty())
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
    }*/

	void UsbConnection::disconnect() {
		usbCommsExit();
	}

    std::vector<char> UsbConnection::receive_data(int sockfd) {
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

    void UsbConnection::sendData(const std::vector<char>& data, size_t size, int sockfd) {
        USBResponse response {
            size,
            (void*)data.data(),
        };

		usbCommsWrite(&response, 4);
        Logger::logToFile("Send response size: " + std::to_string(response.size));
		if (response.size > 0) {
            usbCommsWrite(response.data, response.size);
		}
    }
}
