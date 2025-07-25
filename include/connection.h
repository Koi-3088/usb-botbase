#pragma once

#include "defines.h"
#include "commandHandler.h"
#include <switch.h>

namespace Connection {
	class ConnectionHandler {
	public:
		ConnectionHandler() {}
		virtual ~ConnectionHandler() {}

	public:
		virtual Result initialize(Result& res) = 0;
		virtual bool connect() = 0;
		virtual void run() = 0;
		virtual void disconnect() = 0;
		virtual int receiveData(int sockfd = 0) = 0;
		virtual int sendData(const char* data, size_t data_size, int sockfd = 0) = 0;
	};
}
