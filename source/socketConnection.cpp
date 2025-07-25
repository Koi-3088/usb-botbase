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
#include <sys/select.h>
#include <netinet/in.h>

namespace SocketConnection {
	using namespace Util;
	using namespace SbbLog;
	using namespace CommandHandler;
	using namespace ControllerCommands;

	Result SocketConnection::initialize(Result& res) {
		const SocketInitConfig cfg = {
			0x800, //tcp_tx_buf_size
			0x40000, //tcp_rx_buf_size
		    0x25000, //tcp_tx_buf_max_size
			0x40000, //tcp_rx_buf_max_size

		    0, //udp_tx_buf_size
		    0, //udp_rx_buf_size

		    4, //sb_efficiency

			3, //num_bsd_sessions
			BsdServiceType::BsdServiceType_Auto,
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

        struct linger so_linger {};
        so_linger.l_onoff = 1;
        so_linger.l_linger = 0;
		if (setsockopt(m_tcp.serverFd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger)) < 0) {
			Logger::instance().log("setsockopt(SO_LINGER) error.", std::to_string(errno));
			close(m_tcp.serverFd);
			return -1;
        }

		int opt = 1;
		if (setsockopt(m_tcp.serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
			Logger::instance().log("setsockopt(SO_REUSEADDR) error.", std::to_string(errno));
			close(m_tcp.serverFd);
			return -1;
		}

#ifdef SO_REUSEPORT
		opt = 1;
		if (setsockopt(m_tcp.serverFd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
			Logger::instance().log("setsockopt(SO_REUSEPORT) error.", std::to_string(errno));
			close(m_tcp.serverFd);
			return -1;
		}
#endif

		struct sockaddr_in serverAddr {};
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_addr.s_addr = INADDR_ANY;
		serverAddr.sin_port = htons(m_tcp.port);

		while (bind(m_tcp.serverFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            Logger::instance().log("bind() error, retrying in 1 second...", std::to_string(errno));
			svcSleepThread(5e+6L);
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

				Utils::flashLed();
			}

			struct sockaddr_in clientAddr {};
			socklen_t clientSize = sizeof(clientAddr);
			Logger::instance().log("Waiting for client to connect...", "", true);

			while (true) {
				fd_set readfds;
				FD_ZERO(&readfds);
				FD_SET(m_tcp.serverFd, &readfds);

				if (select(m_tcp.serverFd + 1, &readfds, nullptr, nullptr, nullptr) < 0) {
					if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR) {
						svcSleepThread(1e+6L);
						continue;
					}

					Logger::instance().log("select() error.", std::string(strerror(errno)));
					close(m_tcp.serverFd);
					svcSleepThread(5e+6L);
					if (setupServerSocket() < 0) {
						return false;
					}

					continue;
				}

				if (FD_ISSET(m_tcp.serverFd, &readfds)) {
					while (m_tcp.clientFd == -1) {
						m_tcp.clientFd = accept(m_tcp.serverFd, (struct sockaddr*)&clientAddr, &clientSize);
						if (m_tcp.clientFd == -1) {
							if (errno != EWOULDBLOCK && errno != EAGAIN && errno != EINTR) {
								Logger::instance().log("accept() error.", std::string(strerror(errno)));
								close(m_tcp.serverFd);
								svcSleepThread(5e+6L);
								if (setupServerSocket() < 0) {
									return false;
								}

								break;
							}

							svcSleepThread(1e+6L);
							continue;
						}

						break;
					}
				}

				if (m_tcp.clientFd != -1) {
					break;
                }
			}
		} catch (const std::exception& e) {
            Logger::instance().log("Exception while waiting for client to connect: ", e.what());
			return false;
		} catch (...) {
            Logger::instance().log("Unknown exception while waiting for client to connect.", "Unknown error.");
			return false;
        }

		m_handler->HandleCommand("click", std::vector<std::string> { "UNUSED" });
		m_handler->HandleCommand("detachController", {});
		Logger::instance().log("Client connected.");
		return true;
	}

	void SocketConnection::disconnect() {
		if (m_tcp.clientFd == -1 && m_tcp.serverFd == -1) {
			return;
        }

		Logger::instance().log("Disconnecting WiFi connection...");
		close(m_tcp.serverFd);
		m_tcp.serverFd = -1;
		close(m_tcp.clientFd);
		m_tcp.clientFd = -1;

		if (m_senderThread.joinable()) m_senderThread.join();
		if (m_commandThread.joinable()) m_commandThread.join();
	}

	void SocketConnection::run() {
		m_senderThread = std::thread([&]() {
			while (!m_error) {
				try {
					std::vector<char> buffer;
					while (m_senderQueue.pop(buffer) && !m_error) {
						int sent = sendData(buffer.data(), buffer.size(), m_tcp.clientFd);
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
					break;
				} catch (...) {
					Logger::instance().log("Unknown sender thread exception.", "Unknown error.");
					m_error = true;
					notifyAll();
					break;
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
								if (buffer.back() != '\n') {
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
			m_error = true;
			notifyAll();
        });

		while (!m_error) {
			try {
				if (receiveData(m_tcp.clientFd) < 0) {
                    m_error = true;
					notifyAll();
					break;
				}
			} catch (const std::exception& e) {
				Logger::instance().log("Socket reader thread exception.", e.what());
				m_error = true;
				notifyAll();
				break;
			} catch (...) {
				Logger::instance().log("Unknown socket reader thread exception.", "Unknown error.");
				m_error = true;
				notifyAll();
				break;
			}
		}

        Logger::instance().log("Main socket thread exiting.");
	}

	int SocketConnection::receiveData(int sockfd) {
		constexpr size_t bufSize = 4096;
		char buf[bufSize];

		while (!m_error) {
			ssize_t received = recv(sockfd, buf, bufSize, 0);
			if (received > 0) {
				m_persistentBuffer.append(buf, received);

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
								Controller::ControllerCommand controllerCmd {};
								controllerCmd.parseFromHex(params.front().data());
								m_handler->cqEnqueueCommand(controllerCmd);
							} else if (command == "ping" && params.size() == 1) {
								std::lock_guard<std::mutex> lock(m_senderMutex);
								std::string response = command + " " + params.front() + "\r\n";
                                sendData(response.data(), response.size(), sockfd);
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
				Logger::instance().log("receiveData() client closed the connection.", "", true);
				m_error = true;
				notifyAll();
				return -1;
			} else if (received == -1 && errno != EWOULDBLOCK && errno != EAGAIN) {
				Logger::instance().log("receiveData() recv() error.", std::string(strerror(errno)));
				m_error = true;
				notifyAll();
				return -1;
			}

			if (errno == EWOULDBLOCK || errno == EAGAIN) {
				svcSleepThread(5e+6L);
			}
		}

		return !m_error ? 0 : -1;
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
				Logger::instance().log("sendData(): Failed to send data. Client closed the connection.", "", true);
				m_error = true;
				notifyAll();
				return -1;
			} else if (sent == -1 && errno != EWOULDBLOCK && errno != EAGAIN) {
				Logger::instance().log("sendData(): Failed to send data. send() error.", std::string(strerror(errno)));
				m_error = true;
				notifyAll();
				return -1;
			}

			if (errno == EWOULDBLOCK || errno == EAGAIN) {
				svcSleepThread(5e+6L);
			}
		}

		return !m_error ? total : -1;
	}
}
