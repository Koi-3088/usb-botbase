#pragma once

#include "connection.h"
#include "commandHandler.h"
#include <string>
#include <vector>
#include <memory>
#include <queue>
#include <condition_variable>
#include <atomic>

namespace SocketConnection {
	class SocketConnection : public Connection::ConnectionHandler {
	public:
		SocketConnection() : ConnectionHandler() {
			m_handler = std::make_unique<CommandHandler::Handler>();
		};

		~SocketConnection() override {
			if (m_senderThread.joinable()) {
				m_senderThread.join();
			}

			if (m_readerThread.joinable()) {
				m_readerThread.join();
			}

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
		bool m_dummyClick = true;

		std::thread m_senderThread;
		std::queue<std::vector<char>> m_senderQueue;
		std::mutex m_senderMutex;
		std::condition_variable m_senderCv;

		std::thread m_readerThread;

		std::queue<std::string> m_commandQueue;
		std::mutex m_commandMutex;
		std::condition_variable m_commandCv;

		std::atomic_bool m_running { false };
		std::unique_ptr<CommandHandler::Handler> m_handler;

		int setupServerSocket();
	};
}
