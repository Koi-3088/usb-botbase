#pragma once

#include "connection.h"
#include <string>
#include <unistd.h>
#include <vector>

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
		std::vector<char> receive_data(int sockfd) override;
		void sendData(const std::vector<char>& data, size_t data_size, int sockfd) override;

	private:
		int sockfd;
		const int m_port = 6000;
	};
}
