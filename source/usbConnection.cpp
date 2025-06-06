#include "defines.h"
#include "commandHandler.h"
#include "logger.h"
#include "usbConnection.h"
#include "util.h"
#include <cstring>

namespace UsbConnection {
    using namespace Util;
    using namespace SbbLog;
    using namespace CommandHandler;

    Result UsbConnection::initialize(Result& res) {
        res = usbCommsInitialize();
        return res;
    }

	bool UsbConnection::connect() {
        Utils::flashLed();
        std::vector<std::string> dummyVec(1, "UNUSED");
        std::string persistentBuffer;

        while (appletMainLoop()) {
            if (m_dummyClick) {
                m_handler->HandleCommand("click", dummyVec);
                m_dummyClick = false;
            }

            auto commands = UsbConnection::receiveData(persistentBuffer);
            fflush(stdout);

            if (!commands.empty()) {
                for (const auto& command : commands) {
                    Utils::parseArgs(command, [=](std::string x, const std::vector<std::string>& y) {
                        auto buffer = m_handler->HandleCommand(x, y);
                        if (!buffer.empty()) {
                            if (buffer.back() != '\n') {
                                buffer.push_back('\n');
                            }

                            UsbConnection::sendData(buffer.data(), buffer.size());
                        }
                    });
                }
            }
            else {
                m_dummyClick = true;
            }

            persistentBuffer.clear();
        }

        return false;
	}

    bool UsbConnection::run() {
        return true;
    }

	void UsbConnection::disconnect() {
		usbCommsExit();
	}

    std::vector<std::string> UsbConnection::receiveData(std::string& persistentBuffer, int sockfd) {
        size_t received = 0;
        char buf[256];
        std::vector<std::string> commands;

        while (true) {
            memset(buf, 0, 256);
            received = usbCommsRead(buf, 256);
            if (received > 0) {
                persistentBuffer.append(buf, received);
                size_t pos;
                while ((pos = persistentBuffer.find("\r\n")) != std::string::npos) {
                    commands.push_back(persistentBuffer.substr(0, pos));
                    persistentBuffer.erase(0, pos + 2);
                }

                if (!commands.empty()) {
                    return commands;
                }
            }
            else if (received == -1) {
                Logger::logToFile("receiveData() recv() error.");
                return {};
            }
            else {
                Logger::logToFile("receiveData() client closed the connection.");
                return {};
            }
        }
    }

    int UsbConnection::sendData(const char* buffer, size_t size, int sockfd) {
        size_t total = 0;
		do {
			ssize_t sent = usbCommsWrite((void*)(buffer + total), size - total);
			if (sent == -1) {
				Logger::logToFile("sendData() usbCommsWrite() error.");
                return sent;
			}
            else if (sent == 0) {
                Logger::logToFile("sendData() usbCommsWrite() connection closed.");
                return sent;
            }

            total += sent;
		} while (total < size);

        return total;
    }
}
