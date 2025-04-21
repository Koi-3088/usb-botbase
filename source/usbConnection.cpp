#include "defines.h"
#include "logger.h"
#include "usbConnection.h"
#include "util.h"
#include "commandHandler.h"

namespace UsbConnection {
    using namespace Util;
    using namespace SbbLog;
    using namespace CommandHandler;

    Result UsbConnection::initialize(Result& res) {
        res = usbCommsInitialize();
        return res;
    }

	void UsbConnection::connect() {
        Utils::flashLed();

        Handler handler;
        std::vector<std::string> dummyVec;
        std::string dummyBtn("UNUSED");
        dummyVec.emplace_back(dummyBtn);

        handler.HandleCommand("click", dummyVec);
        dummyVec.clear();
        dummyBtn.clear();

        while (appletMainLoop()) {
            auto buffer = UsbConnection::receiveData();
            if (buffer.empty() || buffer.size() <= 1) {
                continue;
            }

            Utils::parseArgs(buffer, [&](std::string x, const std::vector<std::string>& y) {
                auto buffer = handler.HandleCommand(x, y);
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

    std::vector<char> UsbConnection::receiveData(int sockfd) {
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
