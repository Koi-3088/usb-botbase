#include "defines.h"
#include "logger.h"
#include "ntp.h"
#include <arpa\inet.h>
#include <array>
#include <netdb.h>
#include <sys\socket.h>
#include <sys\time.h>
#include <unistd.h>
#include <switch.h>

namespace NTP {
	using namespace SbbLog;

    time_t NTPClient::getTime() {
        std::array<uint8_t, 48> packet {};
        packet[0] = 0b11100011; // Unsynchronized, NTP ver 4, client

        addrinfo hints {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;

        addrinfo* res = nullptr;
        if (getaddrinfo(m_ntp_server, m_ntp_port, &hints, &res) != 0 || res == nullptr) {
			Logger::Logger::logToFile("getaddrinfo() failed: " + std::string(gai_strerror(errno)));
            return 0;
        }

        Result rc = socketInitializeDefault();
		if (R_FAILED(rc)) {
			Logger::Logger::logToFile("socketInitializeDefault() failed: " + std::to_string(rc));
			freeaddrinfo(res);
			return 0;
		}

        int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd < 0) {
            Logger::Logger::logToFile("socket() failed: " + std::to_string(errno));
            freeaddrinfo(res);
            return 0;
        }

        timeval timeout {
            timeout.tv_sec = 5,
            timeout.tv_usec = 0,
        };

        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
			Logger::Logger::logToFile("setsockopt() failed.");
            close(sockfd);
            freeaddrinfo(res);
            return 0;
        }

        if (sendto(sockfd, packet.data(), packet.size(), 0, res->ai_addr, res->ai_addrlen) <= 0) {
            Logger::Logger::logToFile("sendto() failed.");
            close(sockfd);
            freeaddrinfo(res);
			return 0;
        }

        sockaddr_storage server_addr {};
        socklen_t server_addr_len = sizeof(server_addr);
        if (recvfrom(sockfd, packet.data(), packet.size(), 0, reinterpret_cast<sockaddr*>(&server_addr), &server_addr_len) <= 0) {
            Logger::logToFile("recvfrom() failed.");
            close(sockfd);
            freeaddrinfo(res);
			return 0;
        }

        close(sockfd);
        freeaddrinfo(res);
        socketExit();

        uint32_t seconds = (uint32_t(packet[40]) << 24) |
                           (uint32_t(packet[41]) << 16) |
                           (uint32_t(packet[42]) << 8) |
                           (uint32_t(packet[43])
        );

        if (seconds < m_ntp_delta) {
			Logger::logToFile("Invalid time received from NTP server.");
            return 0;
        }

        return time_t(seconds - m_ntp_delta);
    }
}
