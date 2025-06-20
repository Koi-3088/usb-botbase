#include "defines.h"
#include "logger.h"
#include "socketConnection.h"
#include "commandHandler.h"
#include "util.h"
#include <cstring>
#include <exception>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

namespace SocketConnection {
	using namespace Util;
	using namespace SbbLog;
	using namespace CommandHandler;
	using namespace ControllerCommands;

	Result SocketConnection::initialize(Result& res) {
		res = socketInitializeDefault();
		return res;
	}

	int SocketConnection::setupServerSocket() {
		m_tcp.serverFd = socket(AF_INET, SOCK_STREAM, 0);
		if (m_tcp.serverFd < 0) {
			return -1;
		}

		int opt = 1;
		if (setsockopt(m_tcp.serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
			Logger::logToFile("setsockopt() error.");
			close(m_tcp.serverFd);
			return -1;
		}

		struct sockaddr_in serverAddr {};
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_addr.s_addr = INADDR_ANY;
		serverAddr.sin_port = htons(m_tcp.port);

		while (bind(m_tcp.serverFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
			if (m_error) {
				Logger::logToFile("Socket error detected, exiting setupServerSocket.");
				close(m_tcp.serverFd);
				return -1;
            }

			svcSleepThread(1e+6L);
		}

		if (listen(m_tcp.serverFd, 3) < 0) {
			Logger::logToFile("listen() error.");
			close(m_tcp.serverFd);
			return -1;
		}

		return 0;
	}

	bool SocketConnection::connect() {
		try {
			if (m_tcp.serverFd == -1) {
				if (setupServerSocket() < 0) {
					Logger::logToFile("Socket error.");
					return false;
				}

				m_handler->HandleCommand("click", std::vector<std::string> { "UNUSED" });
				Utils::flashLed();
			}

			size_t retries = 0;
			struct sockaddr_in clientAddr {};
			socklen_t clientSize = sizeof(clientAddr);
			Logger::logToFile("Waiting for client to connect...");

			while (m_tcp.clientFd == -1) {
				if (m_error) {
					Logger::logToFile("Socket connection error detected, exiting connect loop.");
					return false;
                }

				m_tcp.clientFd = accept(m_tcp.serverFd, (struct sockaddr*)&clientAddr, &clientSize);
				if (m_tcp.clientFd == -1) {
					if (m_tcp.serverFd != -1) { // Needed if Switch goes to sleep?
						close(m_tcp.serverFd);
						m_tcp.serverFd = -1;
					}

					setupServerSocket();
					svcSleepThread(1e+6L);
                    retries++;
					if (retries > 60) {
						Logger::logToFile("Timeout while waiting for client to connect.");
                        return false;
					}
				}
			}
		} catch (const std::exception& e) {
            Logger::logToFile(std::string("Exception while waiting for client to connect: ") + e.what());
			return false;
		} catch (...) {
            Logger::logToFile("Unknown exception while waiting for client to connect.");
			return false;
        }

		Logger::logToFile("Client connected.");
		return true;
	}

	void SocketConnection::disconnect() {
		close(m_tcp.serverFd);
        close(m_tcp.clientFd);
        m_error = true;
		notifyAll();
		Logger::logToFile("Disconnected.");
	}

	void SocketConnection::run() {
		m_error = false;
		std::string persistentBuffer;
		m_senderThread = std::thread([&]() {
			try {
				std::unique_lock<std::mutex> lock(m_senderMutex);
				while (!m_error) {
					m_senderCv.wait(lock, [&]() { return !m_senderQueue.empty() || m_error; });
					if (m_error) {
						notifyAll();
                        break;
					}

					if (!m_senderQueue.empty()) {
						auto buffer = std::move(m_senderQueue.front());
						m_senderQueue.pop();

                        //Logger::logToFile("Sending data to client: " + std::string(buffer.data(), buffer.size()));
						int sent = SocketConnection::sendData(buffer.data(), buffer.size(), m_tcp.clientFd);
						if (sent <= 0) {
							Logger::logToFile("sendData() failed or client disconnected.");
                            m_error = true;
							break;
						}
					}
				}
			} catch (const std::exception& e) {
				Logger::logToFile(std::string("Socket sender thread exception: ") + e.what());
				m_error = true;
				notifyAll();
			} catch (...) {
				Logger::logToFile("Unknown socket sender thread exception.");
				m_error = true;
				notifyAll();
			}
		});

		m_commandThread = std::thread([&]() {
			try {
				std::unique_lock<std::mutex> lock(m_commandMutex);
				while (!m_error) {
					m_commandCv.wait(lock, [&]() { return !m_commandQueue.empty() || m_error; });
					if (m_error) {
						notifyAll();
                        break;
					}

					auto command = std::move(m_commandQueue.front());
					m_commandQueue.pop();

					try {
						Utils::parseArgs(command, [&](const std::string& x, const std::vector<std::string>& y) {
							auto buffer = m_handler->HandleCommand(x, y);
							if (!buffer.empty()) {
								if (buffer.back() != '\n') {
									buffer.push_back('\n');
								}

								std::lock_guard<std::mutex> sendLock(m_senderMutex);
								m_senderQueue.push(buffer);
								m_senderCv.notify_one();
							}
						});
					} catch (const std::exception& e) {
						Logger::logToFile(std::string("Command handler exception: ") + e.what());
						m_error = true;
						notifyAll();
						break;
					} catch (...) {
						Logger::logToFile("Unknown command handler exception.");
						m_error = true;
						notifyAll();
						break;
					}

					persistentBuffer.clear();
				}
			} catch (const std::exception& e) {
				Logger::logToFile(std::string("Main command thread exception: ") + e.what());
				m_error = true;
				notifyAll();
			} catch (...) {
				Logger::logToFile("Unknown main command thread exception.");
				m_error = true;
				notifyAll();
			}
		});

		try {
			while (!m_error) {
				bool isRunningPA = m_handler->getIsRunningPA();
				if (!isRunningPA && m_handler->getIsEnabledPA()) {
                    m_handler->startControllerThread(m_senderQueue, m_senderCv);
				}

				auto commands = receiveData(persistentBuffer, m_tcp.clientFd);
				if (commands.empty()) {
					Logger::logToFile("Socket reader thread exiting due to empty command list.");
					m_error = true;
					notifyAll();
					break;
				}

				for (auto& cmd : commands) {
					if (cmd.find("ping") != std::string::npos) {
						sendData(cmd.data(), cmd.length(), m_tcp.clientFd);
						continue;
					}

					if (isRunningPA) {
						if (cmd.find("cqCancel") != std::string::npos) {
							m_handler->cqEnqueueCommand(Controller::ControllerCommand{}, false, true);
							continue;
						}
						
						if (cmd.find("cqReplaceOnNext") != std::string::npos) {
                            m_handler->cqEnqueueCommand(Controller::ControllerCommand{}, true, false);
							continue;
						}
						
						if (cmd.find("cqControllerState") != std::string::npos) {
							Utils::parseArgs(cmd, [&](const std::string& x, const std::vector<std::string>& y) {
								if (y.size() == 1) {
									Controller::ControllerCommand controllerCmd {};
									controllerCmd.parseFromHex(y[0].data());
									m_handler->cqEnqueueCommand(controllerCmd, false, false);
								} else {
									Logger::logToFile("Invalid cqControllerState command format.");
								}
							});
                            continue;
						}
					}

                    std::lock_guard<std::mutex> lock(m_commandMutex);
					m_commandQueue.push(std::move(cmd));
					m_commandCv.notify_all();
				}
			}
		} catch (const std::exception& e) {
			Logger::logToFile(std::string("Socket reader thread exception: ") + e.what());
			m_error = true;
			notifyAll();
		} catch (...) {
			Logger::logToFile("Unknown socket reader thread exception.");
			m_error = true;
			notifyAll();
		}
	}

	std::vector<std::string> SocketConnection::receiveData(std::string& persistentBuffer, int sockfd) {
		size_t received = 0;
		char buf[4096];
		std::vector<std::string> commands;

		while (true) {
			memset(buf, 0, 4096);
			received = recv(sockfd, buf, 4096, 0);

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
			else if (received == (size_t)-1) {
				Logger::logToFile("receiveData() recv() error: " + std::string(strerror(errno)));
                m_error = true;
				notifyAll();
				return {};
			}
			else {
				Logger::logToFile("receiveData() client closed the connection.");
				m_error = true;
				notifyAll();
				return {};
			}
		}
	}

	int SocketConnection::sendData(const char* buffer, size_t size, int sockfd) {
		size_t total = 0;
		do {
			size_t sent = send(sockfd, (void*)(buffer + total), size - total, 0);
			if (sent == (size_t)-1) {
				Logger::logToFile("sendData(): Failed to send data. send() error: " + std::string(strerror(errno)));
				m_error = true;
				notifyAll();
				return sent;
			}
			else if (sent == 0) {
				Logger::logToFile("sendData(): Failed to send data. Client closed the connection.");
				m_error = true;
				notifyAll();
				return sent;
			}

			total += sent;
		} while (total < size);

		return total;
	}
}
