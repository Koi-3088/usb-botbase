#pragma once

#include "lockFreeQueue.h"
#include "connection.h"
#include <string>
#include <vector>
#include <memory>

namespace UsbConnection {
	class UsbConnection : public Connection::ConnectionHandler {
	public:
		UsbConnection() : ConnectionHandler() {
			m_error = false;
			m_handler = std::make_unique<CommandHandler::Handler>();
		};

		~UsbConnection() override {
			m_persistentBuffer.clear();
			m_error = true;
			notifyAll();

			std::lock_guard<std::mutex> sendLock(m_senderMutex);
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
		int receiveData(int sockfd = 0) override;
		int sendData(const char* data, size_t size, int sockfd = 0) override;

	private:
		void notifyAll() {
			m_commandCv.notify_all();
			m_senderCv.notify_all();
			if (m_handler) {
				m_handler->cqNotifyAll();
			}
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

		struct USBResponse {
			u64 size;
			void* data;
		};
	};
}
