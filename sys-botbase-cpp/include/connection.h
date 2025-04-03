#pragma once

#include <vector>
#include <stdio.h>
#include <switch.h>
#include "util.h"

namespace Connection {
	class ConnectionHandler {
	public:
		ConnectionHandler() {}
		virtual ~ConnectionHandler() {}

		virtual Result initialize(Result& res) = 0;
		virtual void connect() = 0;
		virtual void disconnect() = 0;
		virtual std::vector<char> receive_data(int sockfd = 0) = 0;
		virtual void sendData(const std::vector<char>& data, size_t data_size, int sockfd = 0) = 0;

	protected:
		virtual int argmain(std::string cmd, const std::vector<std::string>&, int sockfd = 0) = 0;

	protected:
		const int m_port = 6000;
	};
}
