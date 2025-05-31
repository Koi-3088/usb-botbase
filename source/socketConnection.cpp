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

	bool SocketConnection::connect() {
		if (m_tcp.serverFd == -1) {
			m_tcp.serverFd = setupServerSocket();
			if (m_tcp.serverFd < 0) {
				Logger::logToFile("Socket error.");
				return false;
			}

			Utils::flashLed();
		}

		struct sockaddr_in clientAddr {};
		socklen_t clientSize = sizeof(clientAddr);
		Logger::logToFile("Waiting for client to connect...");

		while (m_tcp.clientFd == -1) {
			m_tcp.clientFd = accept(m_tcp.serverFd, (struct sockaddr*)&clientAddr, &clientSize);
			if (m_tcp.clientFd == -1) {
				svcSleepThread(1e+6L);
			}
		}

		Logger::logToFile("Client connected.");
		return true;
	}

	bool SocketConnection::run() {
		m_error = false;
		std::string persistentBuffer;
		m_senderThread = std::thread([&]() {
			try {
				std::unique_lock<std::mutex> lock(m_senderMutex);
				while (!m_error) {
					m_senderCv.wait(lock, [&]() { return !m_senderQueue.empty() || m_error; });
					if (m_error) break;

					if (!m_senderQueue.empty()) {
						auto buffer = std::move(m_senderQueue.front());
						m_senderQueue.pop();
						lock.unlock();

						int sent = SocketConnection::sendData(buffer, buffer.size(), m_tcp.clientFd);
						if (sent <= 0) {
							m_error = true;
							notifyAll();
							break;
						}
						lock.lock();
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
				while (!m_error) {
					std::unique_lock<std::mutex> lock(m_commandMutex);
					m_commandCv.wait(lock, [&]() { return !m_commandQueue.empty() || m_error; });
					if (m_error) break;

					auto command = std::move(m_commandQueue.front());
					m_commandQueue.pop();
					lock.unlock();

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
					lock.lock();
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
				if (!m_handler->getIsRunningPA() && m_handler->getIsEnabledPA()) {
                    m_handler->startControllerThread(m_senderQueue, m_senderMutex, m_senderCv, m_error);
				}

				auto commands = receiveData(persistentBuffer, m_tcp.clientFd);
				if (commands.empty()) {
					m_error = true;
					notifyAll();
					break;
				}

				std::lock_guard<std::mutex> lock(m_commandMutex);
				for (auto& cmd : commands) {
					m_commandQueue.push(std::move(cmd));
				}

				m_commandCv.notify_one();
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

		notifyAll();
		if (m_senderThread.joinable()) m_senderThread.join();
		if (m_commandThread.joinable()) m_commandThread.join();

		close(m_tcp.clientFd);
		m_tcp.clientFd = -1;
		Logger::logToFile("Client disconnected.");
		return m_error;
	}

	void SocketConnection::disconnect() {
		close(m_tcp.clientFd);
		close(m_tcp.serverFd);
		m_tcp.serverFd = -1;
		m_tcp.clientFd = -1;
	}

	int SocketConnection::setupServerSocket() {
		int sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0) {
			return sockfd;
		}

		int opt = 1;
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
			Logger::logToFile("setsockopt() error.");
			close(sockfd);
			return sockfd;
		}

		struct sockaddr_in serverAddr {};
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_addr.s_addr = INADDR_ANY;
		serverAddr.sin_port = htons(m_tcp.port);

		while (bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
			svcSleepThread(1e+6L);
		}

		if (listen(sockfd, 3) < 0) {
			Logger::logToFile("listen() error.");
			close(sockfd);
			return sockfd;
		}

		return sockfd;
	}

	std::vector<std::string> SocketConnection::receiveData(std::string& persistentBuffer, int sockfd) {
		size_t received = 0;
		char buf[256];
		std::vector<std::string> commands;

		while (true) {
			memset(buf, 0, 256);
			received = recv(sockfd, buf, 256, 0);

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
				return {};
			}
			else {
				Logger::logToFile("receiveData() client closed the connection.");
				return {};
			}
		}
	}

	int SocketConnection::sendData(std::vector<char>& buffer, size_t size, int sockfd) {
		size_t total = 0;
		do {
			ssize_t sent = send(sockfd, (void*)(buffer.data() + total), size - total, 0);
			if (sent == -1) {
				Logger::logToFile("sendData(): Failed to send data. send() error: " + std::string(strerror(errno)));
				return sent;
			}
			else if (sent == 0) {
				Logger::logToFile("sendData(): Failed to send data. Client closed the connection.");
				return sent;
			}

			total += sent;
		} while (total < size);

		buffer.clear();
		return total;
	}
}
