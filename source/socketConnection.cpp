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
//#include <chrono>

namespace SocketConnection {
	using namespace Util;
	using namespace SbbLog;
	using namespace CommandHandler;

	Result SocketConnection::initialize(Result& res) {
		res = socketInitializeDefault();
		return res;
	}

	void SocketConnection::connect() {
		Utils::flashLed();

		int sockfd = setupServerSocket();
		if (sockfd < 0) {
			Logger::logToFile("Socket error.");
			return;
		}

		std::vector<std::string> dummyVec(1, "UNUSED");
		struct sockaddr_in clientAddr {};
		socklen_t clientSize = sizeof(clientAddr);
		Logger::logToFile("Waiting for client to connect...");

		while (appletMainLoop()) {
			int clientfd = accept(sockfd, (struct sockaddr*)&clientAddr, &clientSize);
			if (clientfd < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
				svcSleepThread(1e+6L);
				continue;
			}
			else if (clientfd < 0) {
				Logger::logToFile("Socket connect() accept() failed.");
				break;
			}

			Logger::logToFile("Client connected...");
			std::string persistentBuffer;
			m_running = true;

			m_readerThread = std::thread([&]() {
				try {
					while (m_running) {
						auto commands = receiveData(persistentBuffer, clientfd);
						if (commands.empty()) {
							m_running = false;
							m_commandCv.notify_all();
							break;
						}

						{
							std::lock_guard<std::mutex> lock(m_commandMutex);
							for (auto& cmd : commands) {
								m_commandQueue.push(std::move(cmd));
							}
						}

						m_commandCv.notify_one();
					}
				} catch (const std::exception& e) {
					Logger::logToFile(std::string("Socket reader thread exception: ") + e.what());
					m_running = false;
					m_commandCv.notify_all();
				} catch (...) {
					Logger::logToFile("Unknown socket reader thread exception.");
					m_running = false;
					m_commandCv.notify_all();
				}
			});

			m_senderThread = std::thread([&]() {
				try {
					while (m_running) {
						std::unique_lock<std::mutex> lock(m_senderMutex);
						m_senderCv.wait(lock, [&]() { return !m_senderQueue.empty() || !m_running; });
						while (!m_senderQueue.empty()) {
							auto buffer = std::move(m_senderQueue.front());
							m_senderQueue.pop();
							lock.unlock();
							SocketConnection::sendData(buffer, buffer.size(), clientfd);
							lock.lock();
						}
					}
				}
				catch (const std::exception& e) {
					Logger::logToFile(std::string("Socket sender thread exception: ") + e.what());
					m_running = false;
					m_senderCv.notify_all();
				}
				catch (...) {
					Logger::logToFile("Unknown socket sender thread exception.");
					m_running = false;
					m_senderCv.notify_all();
				}
			});

			try {
				while (m_running) {
					if (m_dummyClick) {
						m_handler->HandleCommand("click", dummyVec);
						m_dummyClick = false;
					}

					std::unique_lock<std::mutex> lock(m_commandMutex);
					m_commandCv.wait(lock, [&]() { return !m_commandQueue.empty() || !m_running; });
					while (!m_commandQueue.empty()) {
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
							m_running = false;
							m_senderCv.notify_all();
							m_commandCv.notify_all();
							break;
						} catch (...) {
							Logger::logToFile("Unknown command handler exception.");
							m_running = false;
							m_senderCv.notify_all();
							m_commandCv.notify_all();
							break;
						}

						lock.lock();
					}

					persistentBuffer.clear();
				}
			} catch (const std::exception& e) {
				Logger::logToFile(std::string("Main command loop exception: ") + e.what());
				m_running = false;
				m_senderCv.notify_all();
				m_commandCv.notify_all();
			} catch (...) {
				Logger::logToFile("Unknown main command loop exception.");
				m_running = false;
				m_senderCv.notify_all();
				m_commandCv.notify_all();
			}

			m_running = false;
			m_senderCv.notify_all();
			m_commandCv.notify_all();
			if (m_readerThread.joinable()) {
				m_readerThread.join();
			}

			if (m_senderThread.joinable()) {
				m_senderThread.join();
			}

			close(clientfd);
			Logger::logToFile("Client disconnected.");
		}

		Logger::logToFile("Closing server connection...");
		close(sockfd);
	}

	void SocketConnection::disconnect() {
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
		serverAddr.sin_port = htons(m_port);

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

	void SocketConnection::sendData(std::vector<char>& buffer, size_t size, int sockfd) {
		size_t total = 0;
		do {
			ssize_t sent = send(sockfd, (void*)(buffer.data() + total), size - total, 0);
			if (sent == -1) {
				Logger::logToFile("sendData(): Failed to send data. send() error: " + std::string(strerror(errno)));
				break;
			}
			else if (sent == 0) {
				Logger::logToFile("sendData(): Failed to send data. Client closed the connection.");
				break;
			}

			total += sent;
		} while (total < size);

		buffer.clear();
	}
}
