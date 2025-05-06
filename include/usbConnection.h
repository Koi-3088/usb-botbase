#pragma once

#include "connection.h"
#include "commandHandler.h"
#include <string>
#include <vector>
#include <memory>

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
		void connect() override;
		void disconnect() override;
		std::vector<std::string> receiveData(std::string& persistentBuffer, int sockfd = 0) override;
		void sendData(std::vector<char>& data, size_t size, int sockfd = 0) override;

	private:
		std::unique_ptr<CommandHandler::Handler> m_handler;

		struct USBResponse {
			u64 size;
			void* data;
		};
	};
}
