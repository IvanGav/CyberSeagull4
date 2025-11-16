#pragma once

#ifdef _WIN32
#include "Windows.h"
#define ioctl ioctlsocket
#define close closesocket
#else
#include <sys/socket.h>
#define INVALID_SOCKET (-1)
#endif

#define CNET_RECV_BUF_SIZE (1024 * 32)

namespace CNet {

enum ToClientType {
	TO_CLIENT_HELLO
};

enum ToServerType {
	TO_SERVER_HELLO
};

struct Client {
	char recvBuf[CNET_RECV_BUF_SIZE];
};

bool do_nework_client() {
#ifdef _WIN32
	WSAData wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("WSAStartup failed\n");
		return false;
	}
#endif
	struct addrinfo* addrInfoResult;
	struct addrinfo hints = { 0 };
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	if (getaddrinfo("127.0.0.1", "1951", &hints, &addrInfoResult) != 0) {
		printf("getaddrinfo failed\n");
		return false;
	}
	SOCKET server = socket(addrInfoResult->ai_family, addrInfoResult->ai_socktype, addrInfoResult->ai_protocol);
	if (server == INVALID_SOCKET) {
		printf("socket failed\n");
		return false;
	}
	if (connect(server, addrInfoResult->ai_addr, addrInfoResult->ai_addrlen) != 0) {
		printf("connect failed\n");
		return false;
	}
	freeaddrinfo(addrInfoResult);
	u_long trueFlagLong;
	if (ioctl(server, FIONBIO, &trueFlagLong) != 0) {
		printf("ioctl failed\n");
		return false;
	}
	int trueFlagInt = 1;
	if (setsockopt(server, IPPROTO_TCP, TCP_NODELAY, (char*)&trueFlagInt, sizeof(int)) != 0) {
		printf("setsockopt failed\n");
		return false;
	}
	send(server, "duck", 5, 0);
	close(server);

}

bool do_network_server() {
#ifdef _WIN32
	WSAData wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("WSAStartup failed\n");
		return false;
	}
#endif
	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET) {
		printf("socket failed\n");
		return false;
	}
	u_long trueFlagLong;
	if (ioctl(s, FIONBIO, &trueFlagLong) != 0) {
		printf("ioctl failed\n");
		return false;
	}
	int trueFlagInt = 1;
	if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char*)&trueFlagInt, sizeof(int))) {
		printf("setsockopt failed\n");
		return false;
	}
	sockaddr_in service;
	service.sin_family = AF_INET;
	service.sin_port = htons(1951);
	service.sin_addr.s_addr = 127 << 0 | 0 << 8 | 0 << 16 | 1 << 24;
	if (bind(s, (sockaddr*)&service, sizeof(service)) != 0) {
		printf("bind failed\n");
		return false;
	}
	if (listen(s, SOMAXCONN) != 0) {
		printf("listen failed\n");
		return false;
	}
	struct sockaddr_in sockAddr;
	int addrLen = sizeof(sockAddr);
	const int CLIENT_COUNT = 1;
	Client client[CLIENT_COUNT];
	while (true) {
		SOCKET client = accept(s, (sockaddr*)&sockAddr, &addrLen);
		if (client != INVALID_SOCKET) {
			printf("New client\n");
		} else {
			continue;
		}
		char buffer[16];
		int amountReceived = recv(client, buffer, 16, 0);
		if (amountReceived >= 0) {
			printf("Received:\n%s", buffer);
		}
		close(client);
	}
}

}