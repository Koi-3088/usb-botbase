#include "defines.h"
#include "logger.h"
#include "socketConnection.h"
#include "util.h"
#include <cstring>
#include <exception>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <chrono>

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
		m_tcp.serverFd = setupServerSocket();
		if (m_tcp.serverFd < 0) {
			Logger::logToFile("Socket error.");
			return false;
		}

		struct sockaddr_in clientAddr {};
		socklen_t clientSize = sizeof(clientAddr);
		Logger::logToFile("Waiting for client to connect...");

		Utils::flashLed();
		while (m_tcp.clientFd == -1) {
			m_tcp.clientFd = accept(m_tcp.serverFd, (struct sockaddr*)&clientAddr, &clientSize);
			if (m_tcp.clientFd < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
				svcSleepThread(1e+6L);
				continue;
			} else if (m_tcp.clientFd < 0) {
				Logger::logToFile("Socket connect() accept() failed.");
				return false;
			}
		}

		Logger::logToFile("Client connected.");
		return true;
	}

	bool SocketConnection::run() {
		std::string persistentBuffer;
		m_readerThread = std::thread([&]() {
			try {
				while (!m_error) {
					auto commands = receiveData(persistentBuffer, m_tcp.clientFd);
					if (commands.empty()) {
						m_error = true;
						notifyAll();
						break;
					}

					{
						std::lock_guard<std::mutex> lock(m_commandMutex);
						if (Controller::m_replaceOnNext) {
							Controller::m_replaceOnNext = false;
							std::queue<std::string> tmp;
							m_commandQueue.swap(tmp);
							Controller::m_nextStateChange = WallClock::min();
							m_commandCv.notify_all();
						}

						if (Controller::m_nextStateChange == WallClock::max()) {
							Controller::m_nextStateChange = WallClock::min();
							m_commandCv.notify_all();
						}

						for (auto& cmd : commands) {
							m_commandQueue.push(std::move(cmd));
						}
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
		});

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
				std::unique_lock<std::mutex> lock(m_commandMutex);
				while (!m_error) {
					if (m_handler->getIsEnabledPA()) {
						controllerLoopPA(lock);
						m_error = true;
						break;
					}

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

		while (!m_error) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		notifyAll();
		if (m_readerThread.joinable()) m_readerThread.join();
		if (m_senderThread.joinable()) m_senderThread.join();
		if (m_commandThread.joinable()) m_commandThread.join();

		Logger::logToFile("Client disconnected.");
		return m_error;
	}

	void SocketConnection::disconnect() {
		close(m_tcp.clientFd);
		close(m_tcp.serverFd);
		socketExit();
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

	void SocketConnection::controllerLoopPA(std::unique_lock<std::mutex>& commandLock) {
		const std::chrono::microseconds EARLY_WAKE(500);
		Controller::ControllerCommand cmd{};
		char buf[sizeof(cmd)];

		while (!m_error) {
			m_commandCv.wait_until(commandLock, Controller::m_nextStateChange - EARLY_WAKE, [&]() { return !m_commandQueue.empty() || m_error; });
			if (m_error) {
				break;
			}

			WallClock now = std::chrono::steady_clock::now();
			if (now >= Controller::m_nextStateChange) {
				if (m_commandQueue.empty()) {
					Controller::m_currentState.clear();
					cmd.state = Controller::m_currentState;
					cmd.writeToHex(buf);
					std::string hex(buf);

					std::vector<std::string> vec { hex };
					auto buffer = m_handler->HandleCommand("clickCC", vec);
					if (!buffer.empty()) {
						if (buffer.back() != '\n') buffer.push_back('\n');
						std::lock_guard<std::mutex> sendLock(m_senderMutex);
						m_senderQueue.push(buffer);
						m_senderCv.notify_all();
					}

					Controller::m_nextStateChange = WallClock::max();
				} else {
					auto command = std::move(m_commandQueue.front());
					m_commandQueue.pop();
					commandLock.unlock();

					if (Controller::m_nextStateChange == WallClock::min()) {
						Controller::m_nextStateChange = now;
					}

					Utils::parseArgs(command, [&](const std::string& x, const std::vector<std::string>& y) {
						if (x == "stopAll") {
							Controller::m_nextStateChange = WallClock::min();
							std::queue<std::string> tmp;
							m_commandQueue.swap(tmp);
							notifyAll();
						} else if (x == "replaceOnNext") {
							Controller::m_replaceOnNext = true;
							notifyAll();
						} else {
							auto buffer = m_handler->HandleCommand(x, y);
							if (!buffer.empty()) {
								if (buffer.back() != '\n') {
									buffer.push_back('\n');
								}

								std::lock_guard<std::mutex> sendLock(m_senderMutex);
								m_senderQueue.push(buffer);
								notifyAll();
							}
						}
					});

					commandLock.lock();
				}

				m_commandCv.notify_all();
			}

			if (now + EARLY_WAKE < Controller::m_nextStateChange) {
				continue;
			}
		}

		Controller::m_currentState.clear();
		cmd.state = Controller::m_currentState;
		cmd.writeToHex(buf);
		std::string hex(buf);

		std::vector<std::string> vec{ hex };
		auto buffer = m_handler->HandleCommand("clickCC", vec);
		if (!buffer.empty()) {
			if (buffer.back() != '\n') {
				buffer.push_back('\n');
			}

			std::lock_guard<std::mutex> sendLock(m_senderMutex);
			m_senderQueue.push(buffer);
			m_senderCv.notify_all();
		}
	}
}
