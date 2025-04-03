#pragma once

#include <switch.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <iostream> 
#include "connection.h"
#include "util.h"

namespace SocketConnection {
	class SocketConnection : public Connection::ConnectionHandler {
	public:
		SocketConnection() : ConnectionHandler() {}
		~SocketConnection() {
			close(sockfd);
		}

		Result initialize(Result& res) override;
		void connect() override;
		void disconnect() override;
		std::vector<char> receive_data(int sockfd) override;
		void sendData(const std::vector<char>& data, size_t data_size, int sockfd) override;

	protected:
		int argmain(std::string cmd, const std::vector<std::string>&, int sockfd) override;

	private:
		int sockfd;
	};
}
