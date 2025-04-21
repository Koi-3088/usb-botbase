#pragma once

#include "connection.h"
#include <string>
#include <vector>

namespace UsbConnection {
	class UsbConnection : public Connection::ConnectionHandler {
	public:
		UsbConnection() : ConnectionHandler() {}
		~UsbConnection() override {}

	public:
		Result initialize(Result& res) override;
		void connect() override;
		void disconnect() override;
		std::vector<char> receiveData(int sockfd = 0) override;
		void sendData(const std::vector<char>& data, size_t size, int sockfd = 0) override;

	private:
		struct USBResponse {
			u64 size;
			void* data;
		};
	};
}
