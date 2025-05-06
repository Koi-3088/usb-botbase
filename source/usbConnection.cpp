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

        std::string persistentBuffer;

        while (appletMainLoop()) {
            auto commands = UsbConnection::receiveData(persistentBuffer);
            for (const auto& command : commands) {
                Utils::parseArgs(command, [=](std::string x, const std::vector<std::string>& y) {
                    auto sendBuffer = m_handler->HandleCommand(x, y);
                    if (sendBuffer.empty()) {
                        return;
                    }

                    Logger::logToFile("Sending data...");
                    UsbConnection::sendData(sendBuffer, sendBuffer.size());
                    });
            }

            //svcSleepThread(1e+6L);
        }
	}

	void UsbConnection::disconnect() {
		usbCommsExit();
	}

    std::vector<std::string> UsbConnection::receiveData(std::string& persistentBuffer, int sockfd) {
        size_t size = 0;
        Logger::logToFile("Size before read: " + std::to_string(size));
        return {};
        /*int len = usbCommsRead(&size, sizeof(size));
        if (len <= 0) {
            return {};
        }

        Logger::logToFile("Size after initial read: " + std::to_string(size));
		std::vector<std::string> buffer(size);
        len = usbCommsRead((void*)buffer.data(), size);
        if (size - 2 > buffer.size() || len <= 0) {
            return buffer;
        }

        Logger::logToFile("Read buffer: " + std::string(buffer.begin(), buffer.end()));
        return buffer;*/
    }

    void UsbConnection::sendData(std::vector<char>& data, size_t size, int sockfd) {
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
