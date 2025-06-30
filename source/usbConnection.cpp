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

        std::string persistentBuffer;
        while (appletMainLoop()) {
            auto commands = UsbConnection::receiveData(persistentBuffer);
            fflush(stdout);

            if (!commands.empty()) {
                for (const auto& command : commands) {
                    Utils::parseArgs(command, [&](std::string x, const std::vector<std::string>& y) {
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
        }

        return false;
	}

    void UsbConnection::run() {
        return;
    }

	void UsbConnection::disconnect() {
		usbCommsExit();
	}

    std::vector<std::string> UsbConnection::receiveData(std::string& persistentBuffer, int sockfd) { // More than likely broken, just for compilation purposes
        constexpr size_t bufSize = 1024;
        std::vector<std::string> commandBuf;
        char buf[bufSize];

        while (true) {
            ssize_t received = usbCommsRead(buf, bufSize);
            if (received > 0) {
                persistentBuffer.append(buf, received);
                size_t pos;
                while ((pos = persistentBuffer.find("\r\n")) != std::string::npos) {
                    commandBuf.push_back(persistentBuffer.substr(0, pos));
                    persistentBuffer.erase(0, pos + 2);
                }

                if (!commandBuf.empty()) {
                    break;
                }
            } else if (received == 0) {
                Logger::logToFile("receiveData() client closed the connection.");
                //m_error = true;
                //notifyAll();
                return {};
            } else {
                Logger::logToFile("receiveData() recv() error: " + std::string(strerror(errno)));
                //m_error = true;
                //notifyAll();
                return {};
            }
        }

        return commandBuf;
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
