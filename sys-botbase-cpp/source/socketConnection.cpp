#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <arpa/inet.h>
#include <sys/unistd.h>
#include <poll.h>
#include "defines.h"
#include "socketConnection.h"

namespace SocketConnection {
	bool SocketConnection::initialize() {
		return R_FAILED(socketInitializeDefault());
	}

	void SocketConnection::connect() {

	}

	void SocketConnection::disconnect() {
		socketExit();
	}

	int SocketConnection::sendData(void* data, u64 length, int sockfd) {
		return 0;
	}
}
