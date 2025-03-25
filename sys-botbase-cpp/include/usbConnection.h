#pragma once

#include <switch.h>
#include "connection.h"

namespace UsbConnection {
	class UsbConnection : public Connection::ConnectionHandler {
	public:
		UsbConnection() {}
		~UsbConnection() {}

		bool initialize() override;
		void connect() override;
		void disconnect() override;
		int sendData(void* data, u64 length, int sockfd = 0) override;

	private:
		typedef struct
		{
			u64 size;
			void* data;
		} USBResponse;
	};
}
