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

        CommandHandler command;
        while (appletMainLoop()) {
            auto buffer = UsbConnection::receive_data();
            if (buffer.empty() || buffer.size() <= 1) {
                continue;
            }

            Utils::parseArgs(buffer, [&](std::string x, const std::vector<std::string>& y) {
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

	void UsbConnection::disconnect() {
		usbCommsExit();
	}

    std::vector<char> UsbConnection::receive_data(int sockfd) {
        size_t size = 0;
        Logger::logToFile("Size before read: " + std::to_string(size));

        int len = usbCommsRead(&size, sizeof(size));
        if (len <= 0) {
            svcSleepThread(1e+6L);
            return std::vector<char>();
        }

        auto buffer = std::vector<char>(size + 1);
        Logger::logToFile("Size after initial read: " + std::to_string(size));

        len = usbCommsRead((void*)buffer.data(), size);
        if (size - 2 > buffer.size() || len <= 0) {
            svcSleepThread(1e+6L);
            return std::vector<char>();
        }

        // Can yeet this if client adds line terminators. No more crlf checks.
        //buffer[size - 1] = '\n';
        //buffer[size - 2] = '\r';

        Logger::logToFile("Read buffer: " + std::string(buffer.data()));

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
