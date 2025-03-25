#pragma once

#include <switch.h>
#include "connection.h"

namespace SocketConnection {
	class SocketConnection : public Connection::ConnectionHandler {
	public:
		SocketConnection() {}
		~SocketConnection() {}

		bool initialize() override;
		void connect() override;
		void disconnect() override;
		int sendData(void* data, u64 length, int sockfd) override;
	};
}
