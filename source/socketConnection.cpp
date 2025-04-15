#include "defines.h"
#include "socketConnection.h"
#include <arpa/inet.h>
#include <iostream> 
#include <poll.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/types.h>

namespace SocketConnection {
	Result SocketConnection::initialize(Result& res) {
		res = socketInitializeDefault();
		return res;
	}

	void SocketConnection::connect() {

	}

	void SocketConnection::disconnect() {
		socketExit();
	}

	std::vector<char> SocketConnection::receive_data(int sockfd) {
		size_t data_size;
		recv(sockfd, &data_size, sizeof(data_size), 0);
		auto buffer = std::vector<char>(data_size);
		size_t total_received = 0;
		while (total_received < data_size) {
			ssize_t received = recv(sockfd, buffer.data() + total_received, data_size - total_received, 0);
			if (received <= 0) {
				perror("recv");
				break;
			}
			total_received += received;
		}
		return buffer;
	}

	void SocketConnection::sendData(const std::vector<char>& data, size_t data_size, int sockfd) {
		size_t total_sent = 0;
		send(sockfd, &data_size, sizeof(data_size), 0);
		while (total_sent < data_size) {
			ssize_t sent = send(sockfd, data.data() + total_sent, data_size - total_sent, 0);
			if (sent == -1) {
				perror("send");
				return;
			}
			total_sent += sent;
		}
	}
}
