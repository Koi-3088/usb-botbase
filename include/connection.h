#pragma once

#include "commandHandler.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <switch.h>
#include <thread>
#include <vector>

namespace Connection {
	class ConnectionHandler {
	public:
		ConnectionHandler() {}
		virtual ~ConnectionHandler() {}

	public:
		virtual Result initialize(Result& res) = 0;
		virtual bool connect() = 0;
		virtual bool run() = 0;
		virtual void disconnect() = 0;
		virtual std::vector<std::string> receiveData(std::string& persistentBuffer, int sockfd = 0) = 0;
		virtual int sendData(std::vector<char>& data, size_t data_size, int sockfd = 0) = 0;

	protected:
		std::thread m_senderThread;
		std::queue<std::vector<char>> m_senderQueue;
		std::mutex m_senderMutex;
		std::condition_variable m_senderCv;

		std::thread m_commandThread;
		std::queue<std::string> m_commandQueue;
		std::mutex m_commandMutex;
		std::condition_variable m_commandCv;

		std::atomic_bool m_error { false };
		std::unique_ptr<CommandHandler::Handler> m_handler;
	};
}
