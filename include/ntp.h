#pragma once

namespace NTP {
	class NTPClient {
	public:
		NTPClient() {
			m_ntp_delta = 2208988800ull;
			m_ntp_server = "time.cloudflare.com";
			m_ntp_port = "123";
		};

		~NTPClient() {};

		time_t getTime();

	private:
		uint64_t m_ntp_delta;
		const char* m_ntp_server;
		const char* m_ntp_port;
	};
}
