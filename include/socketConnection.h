#pragma once

#include "connection.h"
#include <string>
#include <unistd.h>
#include <vector>
#include <arpa/inet.h>
#include <iostream> 
#include <poll.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/types.h>

namespace SocketConnection {
	class SocketConnection : public Connection::ConnectionHandler {
	public:
		SocketConnection() : ConnectionHandler() {}
		~SocketConnection() override {
			close(sockfd);
		}

	public:
		Result initialize(Result& res) override;
		void connect() override;
		void disconnect() override;
		std::vector<char> receiveData(int sockfd) override;
		void sendData(const std::vector<char>& data, size_t data_size, int sockfd) override;

	private:
		int sockfd = -1;
		const int m_port = 6000;

		void setupServerSocket();
		void addToPfds(struct pollfd* pfds[], int clientFd, int* fdCount, int* fdSize);
		void delFromPfds(struct pollfd pfds[], int i, int* fdCount);
	};
}
