// To do: BodyLength+CheckSum compliant FIX msg generator
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <iostream>
#include <string>
#include "SPSCqueue.h"
#include <vector>

// Link against the Windows Sockets library - would modify CMakeLists for proper production
#pragma comment(lib, "Ws2_32.lib")

SQueue<FixMessage, 1024> sendQueue;

int main() {

	// Winsock init, v2.2
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);

	// Create TCP socket socket handle : AF_INET -> IPv4, SOCK_STREAM -> TCP, IPPROTO_TCP -> TCP protocol
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in addr{};
	addr.sin_family = AF_INET; // IPv4
	addr.sin_port = htons(5001); // Port 5001
	// Convert "10.0.1.53" into a binary IP address and store in addr.sin_addr 
	addr.sin_addr.s_addr = htonl((10 << 24) | (0 << 16) | (1 << 8) | 53);
	// inet_pton(AF_INET, "10.0.1.53", &addr.sin_addr);

	// Connect to server
	sock = INVALID_SOCKET;
	while (true) {
		// Winsock does not guarantee that connect() can be retried on the same socket.
		sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == 0) {
			std::cout << "Connected to receiver\n";
			break;
		}
		closesocket(sock);
		_mm_pause();
		Sleep(10);
	}

	std::thread sender([&] {
		FixMessage msg;
		// FIX engines run forever
		while (true) {
			// Non-blocking Attempts to pop a FIX message
			if (sendQueue.dequeue(msg)) {
				// TCP logic simple and fast
				// Sends FIX raw bytes over TCP: No formatting, no parsing
				send(sock, msg.data, (int)msg.len, 0);
			}
			// Backoff while waiting, low latency - bad for multi-thread core sharing
			else _mm_pause();
		}
		});

	// FIX msgs examples
	std::vector<std::string> fix;
	fix.emplace_back(
				"8=FIX.4.4\x01"
				"9=69\x01"
				"35=A\x01"
				"49=CLIENT1\x01"
				"56=SERVER1\x01"
				"34=1\x01"
				"52=20260205-14:32:10.000\x01"
				"98=0\x01"
				"108=30\x01"
				"10=233\x01");

	std::thread producer([&] {
		for (int i = 0; i < 3; ++i) {
			const char* msg_out = fix[0].c_str();

			FixMessage msg{};

			// Formats FIX msg into msg.data
			msg.len = fix[0].size();
			memcpy(msg.data, fix[0].c_str(), msg.len);

			// non blocking enqueue attempt
			while (!sendQueue.enqueue(msg)) _mm_pause();
		}
		});

	producer.join();
	sender.join();
	closesocket(sock);
	WSACleanup();
	return 0;
}
