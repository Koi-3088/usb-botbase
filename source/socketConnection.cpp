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
#include <sys/ioctl.h>
#include <netinet/in.h>

namespace SocketConnection {
	using namespace Util;
	using namespace SbbLog;
	using namespace CommandHandler;
	using namespace ControllerCommands;

	Result SocketConnection::initialize(Result& res) {
		const SocketInitConfig cfg = {
		    0x800, //tcp_tx_buf_size
		    0x800, //tcp_rx_buf_size
		    0x25000, //tcp_tx_buf_max_size
		    0x25000, //tcp_rx_buf_max_size

		    0, //udp_tx_buf_size
		    0, //udp_rx_buf_size

		    1, //sb_efficiency
		};

		res = socketInitialize(&cfg);
		return res;
	}

	int SocketConnection::setupServerSocket() {
		m_tcp.serverFd = socket(AF_INET, SOCK_STREAM, 0);
		if (m_tcp.serverFd < 0) {
            Logger::instance().log("socket() error.", std::to_string(errno));
			return -1;
		}

		int flags = 1;
		if (ioctl(m_tcp.serverFd, FIONBIO, &flags) < 0) {
			Logger::instance().log("ioctl(FIONBIO) error.", std::to_string(errno));
			close(m_tcp.serverFd);
			return -1;
		}

		int opt = 1;
		if (setsockopt(m_tcp.serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
			Logger::instance().log("setsockopt() error.", std::to_string(errno));
			close(m_tcp.serverFd);
			return -1;
		}

		struct sockaddr_in serverAddr {};
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_addr.s_addr = INADDR_ANY;
		serverAddr.sin_port = htons(m_tcp.port);

		int retries = 0;
		while (bind(m_tcp.serverFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            Logger::instance().log("bind() error, retrying in 1 second...", std::to_string(errno));
			svcSleepThread(1e+9L);

			if (++retries > 10) {
				Logger::instance().log("Failed to bind socket after multiple attempts, giving up.", std::to_string(errno));
				close(m_tcp.serverFd);
				socketExit();
				Result rc;
                initialize(rc);
				return -1;
            }
		}

		if (listen(m_tcp.serverFd, 3) < 0) {
			Logger::instance().log("listen() error.", std::to_string(errno));
			close(m_tcp.serverFd);
			return -1;
		}

		return 0;
	}

	bool SocketConnection::connect() {
		try {
			if (m_tcp.serverFd == -1) {
				if (setupServerSocket() < 0) {
					return false;
				}

				m_handler->HandleCommand("click", std::vector<std::string> { "UNUSED" });
				m_handler->HandleCommand("detachController", {});
				Utils::flashLed();
			}

			struct sockaddr_in clientAddr {};
			socklen_t clientSize = sizeof(clientAddr);
			Logger::instance().log("Waiting for client to connect...", "", true);

			while (m_tcp.clientFd == -1) {
				m_tcp.clientFd = accept(m_tcp.serverFd, (struct sockaddr*)&clientAddr, &clientSize);
				if (m_tcp.clientFd == -1) {
					close(m_tcp.serverFd);
					if (setupServerSocket() < 0) {
						return false;
					}

					svcSleepThread(1e+9L);
				}
			}
		} catch (const std::exception& e) {
            Logger::instance().log("Exception while waiting for client to connect: ", e.what());
            m_error = true;
			return false;
		} catch (...) {
            Logger::instance().log("Unknown exception while waiting for client to connect.", "Unknown error.");
            m_error = true;
			return false;
        }

		Logger::instance().log("Client connected.");
		return true;
	}

	void SocketConnection::disconnect() {
        m_error = true;
		close(m_tcp.serverFd);
        close(m_tcp.clientFd);
		Logger::instance().log("Disconnected.");
	}

	void SocketConnection::run() {
		m_error = false;
		m_senderThread = std::thread([&]() {
			while (!m_error) {
				try {
					std::vector<char> buffer;
					while (m_senderQueue.pop(buffer) && !m_error) {
						Logger::instance().log("Sending data to client: " + std::string(buffer.data(), buffer.size() - 1));
						int sent = sendData(buffer.data(), buffer.size(), m_tcp.clientFd);
						if (sent <= 0) {
							Logger::instance().log("sendData() failed or client disconnected.");
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
			m_error = true;
            notifyAll();
		});

		m_commandThread = std::thread([&]() {
			while (!m_error) {
				try {
					std::string command;
					while (m_commandQueue.pop(command) && !m_error) {
						Utils::parseArgs(command, [&](const std::string& x, const std::vector<std::string>& y) {
							auto buffer = m_handler->HandleCommand(x, y);
							if (!m_handler->getIsRunningPA() && m_handler->getIsEnabledPA()) {
								m_handler->startControllerThread(m_senderQueue, m_senderCv, m_senderMutex, m_error);
							}

							if (!buffer.empty()) {
								if (buffer.back() != '\n') {
									buffer.push_back('\n');
								}

								m_senderQueue.push(buffer);
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
            m_error = true;
            notifyAll();
        });

		std::string persistentBuffer;
		while (!m_error) {
			try {
				auto commands = receiveData(persistentBuffer, m_tcp.clientFd);
				if (m_error) {
					break;
				}

				for (auto& cmd : commands) {
					m_commandQueue.push(cmd);
					m_commandCv.notify_one();
				}
			} catch (const std::exception& e) {
				Logger::instance().log("Socket reader thread exception.", e.what());
				m_error = true;
				notifyAll();
			} catch (...) {
				Logger::instance().log("Unknown socket reader thread exception.", "Unknown error.");
				m_error = true;
				notifyAll();
			}
		}

        Logger::instance().log("Main socket thread exiting.");
        m_error = true;
        notifyAll();
	}

	std::vector<std::string> SocketConnection::receiveData(std::string& persistentBuffer, int sockfd) {
		constexpr size_t bufSize = 4096;
		std::vector<std::string> commands;
		char buf[bufSize];

		while (!m_error) {
			ssize_t received = recv(sockfd, buf, bufSize, 0);
			if (received > 0) {
				persistentBuffer.append(buf, received);

				size_t pos;
				while ((pos = persistentBuffer.find("\r\n")) != std::string::npos && !m_error) {
					auto cmd = persistentBuffer.substr(0, pos + 2);
					persistentBuffer.erase(0, pos + 2);

					if (m_handler->getIsRunningPA()) {
						Utils::parseArgs(cmd, [&](const std::string& command, const std::vector<std::string>& params) {
							if (command == "cqCancel") {
								m_handler->cqCancel();
							} else if (command == "cqReplaceOnNext") {
								m_handler->cqReplaceOnNext();
							} else if (command == "cqControllerState") {
								Controller::ControllerCommand controllerCmd {};
								controllerCmd.parseFromHex(params.front().data());
								m_handler->cqEnqueueCommand(controllerCmd);
							} else if (command == "ping" && params.size() == 1) {
								std::lock_guard<std::mutex> lock(m_senderMutex);
								const std::string response = command + " " + params.front() + "\r\n";
                                sendData(response.data(), response.size(), sockfd);
							} else {
								commands.push_back(cmd);
							}
						});
					} else {
						commands.push_back(cmd);
					}
				}

				if (!commands.empty()) {
                    Logger::instance().log("receiveData() received " + std::to_string(commands.size()) + " command(s).");
					break;
				}
			} else if (received == 0) {
				Logger::instance().log("receiveData() client closed the connection.", std::string(strerror(errno)));
				m_error = true;
				notifyAll();
				return {};
			} else {
				if (errno == EWOULDBLOCK || errno == EAGAIN) {
					ssize_t peekResult = recv(sockfd, buf, 1, MSG_PEEK);
					if (peekResult == 0) {
						Logger::instance().log("receiveData() client closed the connection (detected by MSG_PEEK).", std::string(strerror(errno)));
						m_error = true;
						notifyAll();
						break;
					} else if (peekResult < 0 && errno != EWOULDBLOCK && errno != EAGAIN) {
						Logger::instance().log("receiveData() recv(MSG_PEEK) error.", std::string(strerror(errno)));
						m_error = true;
						notifyAll();
						break;
					}

					continue;
				}

				Logger::instance().log("receiveData() recv() error.", std::string(strerror(errno)));
				m_error = true;
				notifyAll();
				return {};
			}
		}

		return commands;
	}

	int SocketConnection::sendData(const char* buffer, size_t size, int sockfd) {
		ssize_t total = 0;
		while (total < (ssize_t)size && !m_error) {
			ssize_t sent = send(sockfd, buffer + total, size - total, 0);
			if (sent > 0) {
				total += sent;
				continue;
			}

			if (sent == 0) {
				Logger::instance().log("sendData(): Failed to send data. Client closed the connection.", std::string(strerror(errno)));
				m_error = true;
				notifyAll();
				return -1;
			} else if (sent == -1 && errno != EWOULDBLOCK && errno != EAGAIN) {
				Logger::instance().log("sendData(): Failed to send data. send() error.", std::string(strerror(errno)));
				m_error = true;
				notifyAll();
				return -1;
			} else {
				char tmp;
				ssize_t peekResult = recv(sockfd, &tmp, 1, MSG_PEEK);
				if (peekResult == 0) {
					Logger::instance().log("sendData(): Client closed the connection (detected by MSG_PEEK).");
					m_error = true;
					notifyAll();
					return -1;
				} else if (peekResult < 0 && errno != EWOULDBLOCK && errno != EAGAIN) {
					Logger::instance().log("sendData(): recv(MSG_PEEK) error: " + std::string(strerror(errno)));
					m_error = true;
					notifyAll();
					return -1;
				}

				continue;
			}
		}

		return total;
	}
}
