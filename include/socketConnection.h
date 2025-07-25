#pragma once

#include "defines.h"
#include "lockFreeQueue.h"
#include "connection.h"
#include <string>
#include <vector>
#include <memory>

namespace SocketConnection {
	class SocketConnection : public Connection::ConnectionHandler {
	public:
		SocketConnection() : ConnectionHandler(), m_tcp(), m_senderQueue(), m_commandQueue() {
			m_error = false;
			m_handler = std::make_unique<CommandHandler::Handler>();
		};

		~SocketConnection() override {
			m_error = true;
			notifyAll();

			m_persistentBuffer.clear();
			m_senderQueue.clear();
			m_commandQueue.clear();

			if (m_senderThread.joinable()) m_senderThread.join();
			if (m_commandThread.joinable()) m_commandThread.join();
			if (m_handler) m_handler.reset();
		};

	public:
		Result initialize(Result& res) override;
		bool connect() override;
		void run() override;
		void disconnect() override;
		int receiveData(int sockfd = 0) override;
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
			if (m_handler) m_handler->cqNotifyAll();
		}

		std::string m_persistentBuffer;

		std::thread m_senderThread;
		LocklessQueue::LockFreeQueue<std::vector<char>> m_senderQueue;
		std::mutex m_senderMutex;
		std::condition_variable m_senderCv;

		std::thread m_commandThread;
		LocklessQueue::LockFreeQueue<std::string> m_commandQueue;
		std::mutex m_commandMutex;
		std::condition_variable m_commandCv;

		std::atomic_bool m_error { false };
		std::unique_ptr<CommandHandler::Handler> m_handler;
	};
}
