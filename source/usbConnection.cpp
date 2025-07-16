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
        while (appletMainLoop()) {
            if (receiveData() < 0) {
                return false;
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

    int UsbConnection::receiveData(int sockfd) { // More than likely broken, just for compilation purposes
        constexpr size_t bufSize = 4096;
        std::string persistentBuffer;
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
                    for (const auto& command : commandBuf) {
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
            } else if (received == 0) {
                Logger::instance().log("receiveData() client closed the connection.", std::string(strerror(errno)));
                //m_error = true;
                //notifyAll();
                return -1;
            } else {
                Logger::instance().log("receiveData() recv() error.", std::string(strerror(errno)));
                //m_error = true;
                //notifyAll();
                return -1;
            }
        }

        return 0;
    }

    int UsbConnection::sendData(const char* buffer, size_t size, int sockfd) {
        size_t total = 0;
		do {
			ssize_t sent = usbCommsWrite((void*)(buffer + total), size - total);
			if (sent == -1) {
				Logger::instance().log("sendData() usbCommsWrite() error.", std::string(strerror(errno)));
                return sent;
			}
            else if (sent == 0) {
                Logger::instance().log("sendData() usbCommsWrite() connection closed.", std::string(strerror(errno)));
                return sent;
            }

            total += sent;
		} while (total < size);

        return total;
    }
}
