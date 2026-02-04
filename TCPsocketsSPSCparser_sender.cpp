// client.cpp - sender

// Windows TCP/IP API
#include <winsock2.h>
#include <ws2tcpip.h>

#include <thread>
#include <iostream>
#include <string>
#include "SPSCqueue.h"

// Link against the Windows Sockets library - would modify CMakeLists for proper production
#pragma comment(lib, "Ws2_32.lib")

SQueue<FixMessage, 1024> sendQueue;

int main() {

	// Winsock init
	WSADATA wsa;
	// Requests version 2.2
	WSAStartup(MAKEWORD(2, 2), &wsa);

	// Create TCP socket socket handle : AF_INET -> IPv4, SOCK_STREAM -> TCP, IPPROTO_TCP -> TCP protocol
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Creates and zero-initializes the address struct
	sockaddr_in addr{};
	addr.sin_family = AF_INET; // IPv4
	addr.sin_port = htons(5001); // Port 5001
	// Convert "127.0.0.1" into a binary IP address and store in addr.sin_addr
	// inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
	// Fix IP 
	addr.sin_addr.s_addr = htonl((10 << 24) | (0 << 16) | (1 << 8) | 53);
	// inet_pton(AF_INET, "10.0.1.53", &addr.sin_addr);

	// Connect to server : Blocks until connection succeeds or fails
	connect(sock, (sockaddr*)&addr, sizeof(addr));
	std::cout << "Connected to server\n"; // log

	// Sender thread (consumer)
	// consumes messages from the queue & writes them to the TCP socket
	std::thread sender([&] {
		FixMessage msg;
		// real FIX engines run forever
		while (true) {
			// Non-blocking Attempts to pop a FIX message
			if (sendQueue.dequeue(msg)) {
				// TCP logic simple and fast
				// Sends the FIX message over TCP w/ No formatting, no parsing — raw bytes - msg.len bytes only (important)
				send(sock, msg.data, (int)msg.len, 0);
			}
			else {
				// Queue empty, reduces CPU power
				_mm_pause();
			}
		}
		});

	// FIX producer thread
	std::thread producer([&] {
		for (int i = 0; i < 10; ++i) {
			// | used instead of SOH for readability
			std::string is = "8=FIX.4.4|35=D|49=CLIENT|56=SERVER|11=dd|55=AAPL|54=1|38=100|10=00" + std::to_string(i) + "|";
			const char* msg_out = is.c_str();

			FixMessage msg{};

			// Todo: REMOVE this and replace with real FIX protocol parsing/creation
			// Formats a FIX message into msg.data - returns the number of bytes written -> stored in msg.len
			// msg.len = snprintf(msg.data, sizeof(msg.data), msg_out, i);
			msg.len = is.size();
			memcpy(msg.data, is.c_str(), msg.len);

			// non blocking enqueue attempt
			while (!sendQueue.enqueue(msg)) {
				// Backoff while waiting for space, Ultra-low latency, busy-waiting but Not friendly for multi-core sharing if other threads could run
				_mm_pause();
			}
		}
		});

	// thread join and clean up
	producer.join();
	sender.join();
	closesocket(sock);
	WSACleanup();
	return 0;
}
