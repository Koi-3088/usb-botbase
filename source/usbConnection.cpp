#include "defines.h"
#include "logger.h"
#include "usbConnection.h"
#include "commandHandler.h"
#include "util.h"
#include <cstring>

namespace UsbConnection {
    using namespace Util;
    using namespace SbbLog;
    using namespace CommandHandler;
    using namespace ControllerCommands;

    Result UsbConnection::initialize(Result& res) {
        res = usbCommsInitialize();
        return res;
    }

	bool UsbConnection::connect() {
        return true;
	}

    void UsbConnection::run() {
        m_error = false;
        Logger::instance().log("Connected...");
        Utils::flashLed();

        m_senderThread = std::thread([&]() {
            while (!m_error) {
                try {
                    std::vector<char> buffer;
                    while (m_senderQueue.pop(buffer) && !m_error) {
                        int sent = sendData(buffer.data(), buffer.size());
                        if (sent <= 0) {
                            Logger::instance().log("sendData() failed or client disconnected.");
                            m_error = true;
                            notifyAll();
                            break;
                        }
                    }

                    std::unique_lock<std::mutex> lock(m_senderMutex);
                    m_senderCv.wait(lock, [&]() { return !m_senderQueue.empty() || m_error; });
                } catch (const std::exception& e) {
                    Logger::instance().log("Sender thread exception.", e.what());
                    m_error = true;
                    notifyAll();
                } catch (...) {
                    Logger::instance().log("Unknown sender thread exception.", "Unknown error.");
                    m_error = true;
                    notifyAll();
                }
            }

            Logger::instance().log("Socket sender thread exiting.");
        });

        m_commandThread = std::thread([&]() {
            while (!m_error) {
                try {
                    std::string command;
                    while (m_commandQueue.pop(command) && !m_error) {
                        Utils::parseArgs(command, [&](const std::string& x, const std::vector<std::string>& y) {
                            auto buffer = m_handler->HandleCommand(x, y);
                            if (!m_handler->getIsRunningPA() && m_handler->getIsEnabledPA()) {
                                m_handler->startControllerThread(m_senderQueue, m_senderCv, m_error);
                            }

                            if (!buffer.empty()) {
                                if (!g_enableBackwardsCompat && buffer.back() != '\n') {
                                    buffer.push_back('\n');
                                }

                                Logger::instance().log("Command processed: " + x + ".");
                                m_senderQueue.push(std::move(buffer));
                                m_senderCv.notify_one();
                            }
                        });
                    }

                    std::unique_lock<std::mutex> lock(m_commandMutex);
                    m_commandCv.wait(lock, [&]() { return !m_commandQueue.empty() || m_error; });
                } catch (const std::exception& e) {
                    Logger::instance().log("Command thread exception: ", e.what());
                    m_error = true;
                    notifyAll();
                    break;
                } catch (...) {
                    Logger::instance().log("Unknown command thread exception.", "Unknown error.");
                    m_error = true;
                    notifyAll();
                    break;
                }
            }

            Logger::instance().log("Command thread exiting.");
        });

        while (!m_error) {
            try {
                if (receiveData() < 0) {
                    m_error = true;
                    break;
                }
            } catch (const std::exception& e) {
                Logger::instance().log("USB reader exception.", e.what());
                m_error = true;
                notifyAll();
            } catch (...) {
                Logger::instance().log("Unknown USB reader exception.", "Unknown error.");
                m_error = true;
                notifyAll();
            }
        }

        Logger::instance().log("Main USB thread exiting.");
    }

	void UsbConnection::disconnect() {
        Logger::instance().log("Disconnecting USB connection...");
        m_error = true;
        notifyAll();

        if (m_senderThread.joinable()) m_senderThread.join();
        if (m_commandThread.joinable()) m_commandThread.join();
        usbCommsExit();
	}

    int UsbConnection::receiveData(int sockfd) {
        while (!m_error) {
            try {
                size_t bufSize = 4096;
                std::vector<char> buf(bufSize);

                if (g_enableBackwardsCompat) {
                    char header[4];
                    size_t headerRead = 0;
                    while (headerRead < 4 && !m_error) {
                        ssize_t readBytes = usbCommsRead(&header + headerRead, 4 - headerRead);
                        if (readBytes <= 0) {
                            svcSleepThread(5e+6L);
                            continue;
                        }

                        headerRead += readBytes;
                    }

                    uint32_t dataSize = 0;
                    std::memcpy(&dataSize, header, 4);
                    buf.resize(dataSize - 2);
                }

                ssize_t received = usbCommsRead((void*)buf.data(), buf.size());
                if (received > 0) {
                    m_persistentBuffer.append(buf.data(), received);
                    fflush(stdout);
                    if (g_enableBackwardsCompat) {
                        m_persistentBuffer.append("\r\n");
                    }

                    size_t pos;
                    while ((pos = m_persistentBuffer.find("\r\n")) != std::string::npos && !m_error) {
                        auto cmd = m_persistentBuffer.substr(0, pos + 2);
                        m_persistentBuffer.erase(0, pos + 2);

                        if (m_handler->getIsRunningPA()) {
                            Utils::parseArgs(cmd, [&](const std::string& command, const std::vector<std::string>& params) {
                                if (command == "cqCancel") {
                                    m_handler->cqCancel();
                                } else if (command == "cqReplaceOnNext") {
                                    m_handler->cqReplaceOnNext();
                                } else if (command == "cqControllerState") {
                                    Controller::ControllerCommand controllerCmd{};
                                    controllerCmd.parseFromHex(params.front().data());
                                    m_handler->cqEnqueueCommand(controllerCmd);
                                } else if (command == "ping" && params.size() == 1) {
                                    std::lock_guard<std::mutex> lock(m_senderMutex);
                                    std::string response = command + " " + params.front() + "\r\n";
                                    sendData(response.data(), response.size());
                                } else {
                                    m_commandQueue.push(std::move(cmd));
                                    m_commandCv.notify_one();
                                }
                            });
                        } else {
                            m_commandQueue.push(std::move(cmd));
                            m_commandCv.notify_one();
                        }
                    }

                    continue;
                } else if (received == 0) {
                    Logger::instance().log("receiveData() client closed the connection.", std::string(strerror(errno)));
                    fflush(stdout);
                    m_error = true;
                    notifyAll();
                    return -1;
                } else {
                    Logger::instance().log("receiveData() recv() error.", std::string(strerror(errno)));
                    fflush(stdout);
                    m_error = true;
                    notifyAll();
                    return -1;
                }
            } catch (...) {
                Logger::instance().log("Exception in receiveData() while reading data.", "Unknown error.");
                fflush(stdout);
                m_error = true;
                notifyAll();
                return -1;
            }
        }

        return 0;
    }

    int UsbConnection::sendData(const char* buffer, size_t size, int sockfd) {
        size_t total = 0;
        USBResponse response {
            size,
            (void*)buffer,
        };

        while (total < size && !m_error) {
            try {
                if (g_enableBackwardsCompat) {
                    usbCommsWrite((void*)&response, 4);
                }

                ssize_t sent = usbCommsWrite(response.data, response.size);
                if (sent == -1) {
                    Logger::instance().log("sendData() usbCommsWrite() error.", std::string(strerror(errno)));
                    m_error = true;
                    return -1;
                } else if (sent == 0) {
                    Logger::instance().log("sendData() usbCommsWrite() connection closed.", std::string(strerror(errno)));
                    m_error = true;
                    return -1;
                }

                total += sent;
            } catch (...) {
                Logger::instance().log("Exception in sendData() while sending data.", "Unknown error.");
                m_error = true;
                return -1;
            }
        }

        return total;
    }
}
