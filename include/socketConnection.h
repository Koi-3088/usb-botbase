#pragma once

#include "connection.h"
#include "commandHandler.h"
#include <string>
#include <vector>
#include <memory>

namespace SocketConnection {
	class SocketConnection : public Connection::ConnectionHandler {
	public:
		SocketConnection() : ConnectionHandler() {
			m_handler = std::make_unique<CommandHandler::Handler>();
		};

		~SocketConnection() override {
			if (m_handler) {
				m_handler.reset();
			}
		};

	public:
		Result initialize(Result& res) override;
		void connect() override;
		void disconnect() override;
		std::vector<std::string> receiveData(std::string& persistentBuffer, int sockfd = 0) override;
		void sendData(std::vector<char>& data, size_t data_size, int sockfd) override;

	private:
		const int m_port = 6000;
		std::unique_ptr<CommandHandler::Handler> m_handler;

		int setupServerSocket();
	};
}
