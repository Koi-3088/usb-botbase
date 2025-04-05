#pragma once

#include "connection.h"
#include <string>
#include <vector>

namespace UsbConnection {
	class UsbConnection : public Connection::ConnectionHandler {
	public:
		UsbConnection() : ConnectionHandler() {}
		~UsbConnection() {}

		Result initialize(Result& res) override;
		void connect() override;
		void disconnect() override;
		std::vector<char> receive_data(int sockfd = 0) override;
		void sendData(const std::vector<char>& data, size_t size, int sockfd = 0) override;

	/*protected:
		int argmain(std::string cmd, const std::vector<std::string>&, int sockfd = 0) override;*/

	private:
		typedef struct
		{
			u64 size;
			void* data;
		} USBResponse;
	};
}
