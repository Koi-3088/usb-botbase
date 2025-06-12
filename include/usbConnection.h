#pragma once

#include "connection.h"
#include <memory>
#include <string>
#include <vector>

namespace UsbConnection {
	class UsbConnection : public Connection::ConnectionHandler {
	public:
		UsbConnection() : ConnectionHandler() {
			m_handler = std::make_unique<CommandHandler::Handler>();
		};

		~UsbConnection() override {
			if (m_handler) {
				m_handler.reset();
			}
		};

	public:
		Result initialize(Result& res) override;
		bool connect() override;
		void run() override;
		void disconnect() override;
		std::vector<std::string> receiveData(std::string& persistentBuffer, int sockfd = 0) override;
		int sendData(const char* data, size_t size, int sockfd = 0) override;

	private:
		std::unique_ptr<CommandHandler::Handler> m_handler;
		bool m_dummyClick = true;

		struct USBResponse {
			u64 size;
			void* data;
		};
	};
}
