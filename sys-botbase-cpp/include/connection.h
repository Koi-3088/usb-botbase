#pragma once

#include <stdio.h>
#include <atomic>
#include <switch.h>

namespace Connection {
	class ConnectionHandler {
	public:
		ConnectionHandler() : m_usb(isUSB()) {}
		virtual ~ConnectionHandler() {}

		virtual bool initialize() = 0;
		virtual void connect() = 0;
		virtual void disconnect() = 0;
		virtual int sendData(void* data, u64 length, int sockfd = 0) = 0;
		bool usb() { return m_usb; }

	private:
		bool isUSB();
		std::atomic<bool> m_usb;

	protected:
		const int m_port = 6000;
	};
}
