#include "commandHandler.h"
#include "defines.h"
#include "logger.h"
#include "socketConnection.h"
#include "util.h"
#include <malloc.h>

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

		Handler handler;
		std::vector<std::string> dummyVec;
		std::string dummyBtn("UNUSED");
		dummyVec.emplace_back(dummyBtn);

		handler.HandleCommand("click", dummyVec);
		dummyVec.clear();
		dummyBtn.clear();

		setupServerSocket();
		if (sockfd < 0) {
			Logger::logToFile("Socket error.");
			return;
		}

		struct sockaddr_in clientAddr {};
		socklen_t clientSize = sizeof(clientAddr);
		int clientFd = -1;

		int fdCount = 1;
		int fdSize = 5;
		struct pollfd* pfds = (struct pollfd*)malloc(sizeof(*pfds) * fdSize);

		pfds[0].fd = sockfd;
		pfds[0].events = POLLIN;
		Logger::logToFile("Waiting for client to connect...");

		while (appletMainLoop()) {
			poll(pfds, fdCount, -1);
			for (int i = 0; i < fdCount; i++) {
				if (pfds[i].revents & (POLLIN | POLLHUP)) {
					if (pfds[i].fd == sockfd) {
						clientFd = accept(sockfd, (struct sockaddr*)&clientAddr, &clientSize);
						if (clientFd != -1) {
							Logger::logToFile("Client connected...");
							addToPfds(&pfds, clientFd, &fdCount, &fdSize);
						}
						else {
							svcSleepThread(1e+9L);
							close(sockfd);
							setupServerSocket();
							pfds[0].fd = sockfd;
							pfds[0].events = POLLIN;
							break;
						}
					}
					else {
						Logger::logToFile("Receiving data...");
						clientFd = pfds[i].fd;
						auto buffer = receiveData(clientFd);

						if (!buffer.empty()) {
							Utils::parseArgs(buffer, [&](std::string x, const std::vector<std::string>& y) {
								auto buffer = handler.HandleCommand(x, y);
								if (buffer.empty()) {
									return;
								}

								fflush(stdout);
								dup2(pfds[i].fd, STDOUT_FILENO);

								Logger::logToFile("Sending data...");
								SocketConnection::sendData(buffer, buffer.size(), clientFd);
							});
						}
						else {
							Logger::logToFile("Buffer empty, closing pfd.");
							close(pfds[i].fd);
							delFromPfds(pfds, i, &fdCount);
							i--;
						}
					}
				}
			}
		}

		Logger::logToFile("Closing server connection...");
		close(sockfd);
	}

	void SocketConnection::disconnect() {
		socketExit();
	}

	void SocketConnection::setupServerSocket() {
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0) {
			return;
		}

		int opt = 1;
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
			Logger::logToFile("setsockopt() error.");
			close(sockfd);
			return;
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
			return;
		}

		Logger::logToFile("Listening on port " + std::to_string(m_port) + "...");
	}

	void SocketConnection::addToPfds(struct pollfd* pfds[], int clientFd, int* fdCount, int* fdSize) {
		if (*fdCount == *fdSize) {
			*fdSize *= 2;

			*pfds = (struct pollfd*)realloc(*pfds, sizeof(**pfds) * (*fdSize));
		}

		(*pfds)[*fdCount].fd = clientFd;
		(*pfds)[*fdCount].events = POLLIN;

		(*fdCount)++;
	}

	void SocketConnection::delFromPfds(struct pollfd pfds[], int i, int* fdCount) {
		pfds[i] = pfds[*fdCount - 1];
		(*fdCount)--;
	}

	std::vector<char> SocketConnection::receiveData(int sockfd) {
		size_t size = 65535;
		auto buffer = std::vector<char>(size);
		int len = recv(sockfd, (void*)buffer.data(), size, 0);
		if (len <= 0) {
			Logger::logToFile("Failed initial data read.");
			return {};
		}

		size_t total = len;
		Logger::logToFile("Read initial data size: " + std::to_string(len));
		while (buffer.data()[total - 1] != '\n') {
			auto received = recv(sockfd, (void*)(buffer.data() + total), size - total, 0);
			if (received <= 0) {
				Logger::logToFile("Failed to receive data.");
				return {};
			}

			total += received;
			Logger::logToFile("Received so far: " + std::to_string(total));
		}

		buffer.data()[total - 1] = 0;
		return buffer;
	}

	void SocketConnection::sendData(const std::vector<char>& buffer, size_t size, int sockfd) {
		size_t total = 0;
		while (total < size) {
			ssize_t sent = send(sockfd, (void*)(buffer.data() + total), size - total, 0);
			if (sent == -1) {
				Logger::logToFile("Failed to send data.");
				return;
			}

			total += sent;
			Logger::logToFile("Sent so far: " + std::to_string(total));

			if (buffer.data()[total - 1] == '\n') {
				break;
			}
		}
	}
}
