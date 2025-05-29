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
						std::lock_guard<std::mutex> lock(m_handler->m_commandMutex);
						for (auto& cmd : commands) {
							m_handler->m_commandQueue.push(std::move(cmd));
						}

						m_handler->m_commandCv.notify_one();
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
                        Logger::logToFile("Sending data to client, size: " + std::to_string(buffer.size()));
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
					if (m_handler->getIsEnabledPA()) {
						controllerLoopPA();
						m_error = true;
                        Logger::logToFile("Exiting command thread.");
						break;
					}

					std::unique_lock<std::mutex> lock(m_handler->m_commandMutex);
					m_handler->m_commandCv.wait(lock, [&]() { return !m_handler->m_commandQueue.empty() || m_error; });
					if (m_error) break;

					auto command = std::move(m_handler->m_commandQueue.front());
					m_handler->m_commandQueue.pop();
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

		m_senderCv.notify_all();
		m_handler->m_commandCv.notify_all();
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

	void SocketConnection::controllerLoopPA() {
		std::unique_lock<std::mutex> lock(m_handler->m_commandMutex);
		const std::chrono::microseconds EARLY_WAKE(500);
		char hex[65] = { 0 };
        Logger::logToFile("Controller loop started.");
		try {
			while (!m_error) {
				memset(hex, 0, 65);
				WallClock now = std::chrono::steady_clock::now();
				std::lock_guard<std::mutex> state(Controller::m_stateMutex);
				if (Controller::m_replaceOnNext) {
					Controller::m_replaceOnNext = false;
					std::queue<std::string> tmp;
					m_handler->m_commandQueue.swap(tmp);
					Controller::m_nextStateChange = WallClock::min();
				}

				if (Controller::m_nextStateChange == WallClock::max()) {
					Controller::m_nextStateChange = WallClock::min();
				}

				if (now >= Controller::m_nextStateChange) {
					if (m_handler->m_commandQueue.empty()) {
						Controller::m_controllerCommand.state.clear();
						Controller::m_controllerCommand.writeToHex(hex);
						std::string cmdStr = "cqControllerState " + std::string(hex) + "\r\n";
						lock.unlock();

						Logger::logToFile("Controller state changed, clearing state: " + std::string(hex));
						Utils::parseArgs(cmdStr, [&](const std::string& x, const std::vector<std::string>& y) {
							auto buffer = m_handler->HandleCommand(x, y);
							if (!buffer.empty()) {
								if (buffer.back() != '\n') {
									buffer.push_back('\n');
								}

								std::lock_guard<std::mutex> sendLock(m_senderMutex);
								m_senderQueue.push(buffer);
								m_senderCv.notify_all();
								//Logger::logToFile("Controller command processed: " + x);
							} else {
								//Logger::logToFile("Controller command returned empty buffer: " + x);
							}
						});
					} else {
						//Logger::logToFile("Processing command in controller loop.");
						auto command = std::move(m_handler->m_commandQueue.front());
						m_handler->m_commandQueue.pop();
						if (Controller::m_nextStateChange == WallClock::min()) {
							Controller::m_nextStateChange = now;
						}

						lock.unlock();
						Utils::parseArgs(command, [&](const std::string& x, const std::vector<std::string>& y) {
							auto buffer = m_handler->HandleCommand(x, y);
							if (!buffer.empty()) {
								if (buffer.back() != '\n') {
									buffer.push_back('\n');
								}

								std::lock_guard<std::mutex> sendLock(m_senderMutex);
								m_senderQueue.push(buffer);
								m_senderCv.notify_all();
								//Logger::logToFile("Controller command processed: " + x);
							} else {
								//Logger::logToFile("Controller command returned empty buffer: " + x);
							}
						});
					}

					//m_handler->m_commandCv.notify_all();
					lock.lock();
					continue;
				}

				if (now + EARLY_WAKE >= Controller::m_nextStateChange) {
					Logger::logToFile("Controller loop early wake, waiting for next state change.");
					continue;
				}

				m_handler->m_commandCv.wait_until(lock, Controller::m_nextStateChange - EARLY_WAKE);
				if (m_error) {
					break;
				}

				//Logger::logToFile("Controller loop woke up, checking for state change.");
			}

			memset(hex, 0, 65);
			Controller::m_controllerCommand.state.clear();
			Controller::m_controllerCommand.writeToHex(hex);

			std::string cmdStr = "cqControllerState " + std::string(hex) + "\r\n";
			lock.unlock();

			Utils::parseArgs(cmdStr, [&](const std::string& x, const std::vector<std::string>& y) {
				auto buffer = m_handler->HandleCommand(x, y);
				if (!buffer.empty()) {
					if (buffer.back() != '\n') {
						buffer.push_back('\n');
					}

					std::lock_guard<std::mutex> sendLock(m_senderMutex);
					m_senderQueue.push(buffer);
					m_senderCv.notify_all();
					//Logger::logToFile("Controller command processed: " + x);
				} else {
					//Logger::logToFile("Controller command returned empty buffer: " + x);
				}
			});

            lock.lock();
		} catch (const std::exception& e) {
			Logger::logToFile(std::string("controllerLoopPA exception: ") + e.what());
			m_error = true;
			m_senderCv.notify_all();
			m_handler->m_commandCv.notify_all();
		} catch (...) {
			Logger::logToFile("Unknown exception in controllerLoopPA.");
			m_error = true;
			m_senderCv.notify_all();
			m_handler->m_commandCv.notify_all();
		}
	}
}
