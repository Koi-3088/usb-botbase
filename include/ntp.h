#pragma once

#include "defines.h"
#include "logger.h"
#include <arpa\inet.h>
#include <array>
#include <cstring>
#include <netdb.h>
#include <sys\socket.h>
#include <sys\time.h>
#include <unistd.h>

namespace NTP {
	using namespace SbbLog;

	class NTPClient {
	public:
		NTPClient() {
			m_ntp_delta = 2208988800ull;
			m_ntp_server = "time.cloudflare.com";
			m_ntp_port = "123";
		};

		~NTPClient() {};

		time_t getTime() {
            std::array<uint8_t, 48> packet{};
            packet[0] = 0b11100011; // Unsynchronized, NTP ver 4, client

            addrinfo hints{};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_DGRAM;
            hints.ai_protocol = IPPROTO_UDP;

            addrinfo* res = nullptr;
            if (getaddrinfo(m_ntp_server, m_ntp_port, &hints, &res) != 0 || res == nullptr) {
                Logger::logToFile("NTP getaddrinfo() failed: ", std::string(gai_strerror(errno)));
                return 0;
            }

            int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
            if (sockfd < 0) {
                Logger::logToFile("NTP socket() failed: ", std::to_string(errno));
                freeaddrinfo(res);
                return 0;
            }

            timeval timeout{
                timeout.tv_sec = 5,
                timeout.tv_usec = 0,
            };

            if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
                Logger::logToFile("NTP setsockopt() failed: ", std::string(strerror(errno)));
                close(sockfd);
                freeaddrinfo(res);
                return 0;
            }

            if (sendto(sockfd, packet.data(), packet.size(), 0, res->ai_addr, res->ai_addrlen) <= 0) {
                Logger::logToFile("NTP sendto() failed or server closed the connection: ", std::string(strerror(errno)));
                close(sockfd);
                freeaddrinfo(res);
                return 0;
            }

            sockaddr_storage server_addr{};
            socklen_t server_addr_len = sizeof(server_addr);
            if (recvfrom(sockfd, packet.data(), packet.size(), 0, reinterpret_cast<sockaddr*>(&server_addr), &server_addr_len) <= 0) {
                Logger::logToFile("NTP recvfrom() failed or server closed the connection: ", std::string(strerror(errno)));
                close(sockfd);
                freeaddrinfo(res);
                return 0;
            }

            close(sockfd);
            freeaddrinfo(res);

            uint32_t seconds = (uint32_t(packet[40]) << 24) |
                (uint32_t(packet[41]) << 16) |
                (uint32_t(packet[42]) << 8) |
                (uint32_t(packet[43])
                    );

            if (seconds < m_ntp_delta) {
                Logger::logToFile("Invalid time received from NTP server.", "", true);
                return 0;
            }

            return time_t(seconds - m_ntp_delta);
		}

	private:
		uint64_t m_ntp_delta;
		const char* m_ntp_server;
		const char* m_ntp_port;
	};
}
