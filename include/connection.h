#pragma once

#include <string>
#include <switch.h>
#include <vector>

namespace Connection {
	class ConnectionHandler {
	public:
		ConnectionHandler() {}
		virtual ~ConnectionHandler() {}

	public:
		virtual Result initialize(Result& res) = 0;
		virtual void connect() = 0;
		virtual void disconnect() = 0;
		virtual std::vector<std::string> receiveData(std::string& persistentBuffer, int sockfd = 0) = 0;
		virtual void sendData(std::vector<char>& data, size_t data_size, int sockfd = 0) = 0;
	};
}
