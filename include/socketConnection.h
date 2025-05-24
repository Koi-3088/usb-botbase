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
			m_error = true;
			notifyAll();
			if (m_readerThread.joinable()) m_readerThread.join();
			if (m_senderThread.joinable()) m_senderThread.join();
			if (m_commandThread.joinable()) m_commandThread.join();

			if (m_handler) {
				m_handler.reset();
			}
		};

	public:
		Result initialize(Result& res) override;
		bool connect() override;
		bool run() override;
		void disconnect() override;
		std::vector<std::string> receiveData(std::string& persistentBuffer, int sockfd = 0) override;
		int sendData(std::vector<char>& data, size_t data_size, int sockfd) override;

	private:
		using WallClock = std::chrono::steady_clock::time_point;
		struct TcpConnection {
			int serverFd = -1;
			int clientFd = -1;
			const int port = 6000;
		};

		TcpConnection m_tcp;

		std::thread m_readerThread;
		std::thread m_senderThread;
		std::thread m_commandThread;

		std::queue<std::vector<char>> m_senderQueue;
		std::mutex m_senderMutex;
		std::condition_variable m_senderCv;

		std::queue<std::string> m_commandQueue;
		std::mutex m_commandMutex;
		std::condition_variable m_commandCv;

		std::atomic_bool m_error { false };
		std::unique_ptr<CommandHandler::Handler> m_handler;

		int setupServerSocket();
		void notifyAll() {
			m_senderCv.notify_all();
			m_commandCv.notify_all();
		}

		void controllerLoopPA(std::unique_lock<std::mutex>& commandLock);
	};
}
