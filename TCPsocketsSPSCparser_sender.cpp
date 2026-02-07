// to do: Separate unit tests
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <iostream>
#include <string>
#include "SPSCqueue.h"
#include <vector>
#include <chrono>

// Link against the Windows Sockets library - modify CMakeLists in production
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

	// valid msg - short
	fix.emplace_back(
		"8=FIX.4.2\x01"
		"9=5\x01"
		"35=0\x01"
		"10=161\x01");

	// valid msg - long
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

	// 1. Standard heartbeat
	fix.emplace_back(
		"8=FIX.4.4\x01"
		"9=5\x01"
		"35=0\x01"
		"10=163\x01");

	// 3. Garbage before message
	fix.emplace_back(
		"garbage" "\x01"
		"8=FIX.4.4" "\x01"
		"9=5" "\x01"
		"35=0" "\x01"
		"10=163" "\x01");

	// Corrupted prefix and recovery: Parser should recover and parse message after "8=FIX"
	fix.emplace_back(
		"junk8=FIX.4.4" "\x01"
		"9=5" "\x01"
		"35=0" "\x01"
		"10=163" "\x01"
	);

	// Message with nested SOH inside value (illegal): parser should reject this and move on
	fix.emplace_back(
		"8=FIX.4.4" "\x01"
		"9=9" "\x01"
		"35=D" "\x01"
		"55=ABC" "\x01" "DEF" "\x01"
		"10=123" "\x01"
	);

	// Partial/incomplete message: parser should wait for the next msg and process them together
	fix.emplace_back(
		"8=FIX.4.4" "\x01"
		"9=5" "\x01"
		"35=0" "\x01"
	);
	fix.emplace_back(
		"10=163" "\x01"
	);


	// Two messages in one
	fix.emplace_back(
		"8=FIX.4.4" "\x01"
		"9=5" "\x01"
		"35=0" "\x01"
		"10=163" "\x01"
		"8=FIX.4.2\x01"
		"9=5\x01"
		"35=0\x01"
		"10=161\x01"
	);

	// empty msg: parser shouldn't be affected
	fix.emplace_back(

	);


	// BodyLength mismatch: paraser should reject
	fix.emplace_back(
		"8=FIX.4.4" "\x01"
		"9=4" "\x01"
		"35=0" "\x01"
		"10=163" "\x01"
	);

	// 7. Wrong checksum: parse should outpout "bad checksum" and continue
	fix.emplace_back(
		"8=FIX.4.4" "\x01"
		"9=5" "\x01"
		"35=0" "\x01"
		"10=165" "\x01"
	);

	// 9. MsgType (35=) must come immediately after BodyLength: parser should reject as malformed
	fix.emplace_back(
		"8=FIX.4.4\x01"
		"9=69\x01"
		"49=CLIENT1\x01"
		"35=A\x01"
		"56=SERVER1\x01"
		"34=1\x01"
		"52=20260205-14:32:10.000\x01"
		"98=0\x01"
		"108=30\x01"
		"10=233\x01");


	std::thread producer([&] {
		for (int i = 0; i < 20; ++i) {
			std::this_thread::sleep_for(std::chrono::milliseconds(11));
			std::string fix_msg = fix[i%fix.size()];
			const char* msg_out = fix_msg.c_str();

			FixMessage msg{};

			// Formats FIX msg into msg.data
			msg.len = fix_msg.size();
			memcpy(msg.data, fix_msg.c_str(), msg.len);

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
