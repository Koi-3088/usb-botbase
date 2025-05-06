#include "defines.h"
#include "logger.h"
#include "socketConnection.h"
#include "util.h"
#include <cstring>
#include <exception>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <sys/errno.h>
#include <sys/socket.h>

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

		std::vector<std::string> dummyVec(1, "UNUSED");
		m_handler->HandleCommand("click", dummyVec);
		dummyVec.clear();

		int sockfd = setupServerSocket();
		if (sockfd < 0) {
			Logger::logToFile("Socket error.");
			return;
		}

		struct sockaddr_in clientAddr {};
		socklen_t clientSize = sizeof(clientAddr);

		std::vector<pollfd> pfds(2);
		pfds[0].fd = sockfd; // server
		pfds[0].events = POLLIN;

		pfds[1].fd = -1; // client
		pfds[1].events = POLLIN;
		Logger::logToFile("Waiting for client to connect...");

		std::string persistentBuffer;

		while (appletMainLoop()) {
			int res = poll(pfds.data(), pfds.size(), -1);
			if (res < 0) {
				Logger::logToFile("poll() failed.");
				svcSleepThread(1e+6L);
				continue;
			}

			if (pfds[0].revents & POLLIN) {
				//Logger::logToFile("Server socket is ready to accept a new connection.");
				pfds[1].fd = accept(sockfd, (struct sockaddr*)&clientAddr, &clientSize);
				if (pfds[1].fd >= 0) {
					Logger::logToFile("Client connected...");
					pfds[1].events = POLLIN;
				}
				else if (pfds[1].fd < 0 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
					continue;
				}
				else {
					//Logger::logToFile("accept() error, resetting the server socket...");
					close(sockfd);
					close(pfds[1].fd);
					svcSleepThread(1e+6L);
					setupServerSocket();
					pfds[0].fd = sockfd;
					pfds[0].events = POLLIN;
					continue;
				}
			}

			if (pfds[1].revents & POLLIN) {
				try {
					//Logger::logToFile("Receiving data...");
					auto commands = receiveData(persistentBuffer, pfds[1].fd);

					fflush(stdout);
					dup2(pfds[1].fd, STDOUT_FILENO);

					if (!commands.empty()) {
						for (const auto& command : commands) {
							Utils::parseArgs(command, [=](const std::string& x, const std::vector<std::string>& y) {
								auto buffer = m_handler->HandleCommand(x, y);
								if (!buffer.empty()) {
									//Logger::logToFile("Sending data...");
									if (buffer.back() != '\n') {
										buffer.push_back('\n');
									}

									SocketConnection::sendData(buffer, buffer.size(), pfds[1].fd);
								}
							});
						}

						persistentBuffer.clear();
					}
					else {
						//Logger::logToFile("Buffer empty, closing pfd.");
						close(pfds[1].fd);
						pfds[1].fd = -1;
						pfds[1].events = POLLIN;
						persistentBuffer.clear();
					}
				}
				catch (const std::exception& e) {
					Logger::logToFile("Exception: " + std::string(e.what()));
				}
			}
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
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
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

		Logger::logToFile("Listening on port " + std::to_string(m_port) + "...");
		return sockfd;
	}

	std::vector<std::string> SocketConnection::receiveData(std::string& persistentBuffer, int sockfd) {
		ssize_t received = 0;
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
			else if (received == -1) {
				Logger::logToFile("receiveData() recv() error.");
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
			if (sent <= 0) {
				buffer.clear();
				Logger::logToFile("sendData(): Failed to send data. send() error or client closed the connection.");
				return;
			}

			total += sent;
			//Logger::logToFile("Sent so far: " + std::to_string(total));
		} while (total < size);

		buffer.clear();
	}
}
