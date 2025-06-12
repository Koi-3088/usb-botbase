#pragma once

#include "connection.h"
#include <string>
#include <vector>
#include <memory>

namespace CommandHandler {
	class Handler;
}

namespace SocketConnection {
	class SocketConnection : public Connection::ConnectionHandler {
	public:
		SocketConnection() : ConnectionHandler(), m_tcp() {
			m_handler = std::make_unique<CommandHandler::Handler>();
		};

		~SocketConnection() override {
            std::lock_guard<std::mutex> lock(m_senderMutex);
			if (m_senderThread.joinable()) m_senderThread.join();

            std::lock_guard<std::mutex> commandLock(m_commandMutex);
			if (m_commandThread.joinable()) m_commandThread.join();
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
		int sendData(const char* data, size_t data_size, int sockfd) override;

	private:
		struct TcpConnection {
			int serverFd = -1;
			int clientFd = -1;
			const int port = 6000;
		};

		TcpConnection m_tcp;

		int setupServerSocket();
		void notifyAll() {
			m_commandCv.notify_all();
            m_senderCv.notify_all();
            m_handler->cqNotifyAll();
			if (m_handler) {
                m_handler->cqNotifyAll();
			}
		}
	};
}
