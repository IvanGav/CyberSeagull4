#pragma once

#ifdef _WIN32
#include <Windows.h>
#define ioctl ioctlsocket
#define close closesocket
#define CNET_SOCK_ERR() WSAGetLastError()
#else
#include <sys/socket.h>
#include <errno.h>
#define INVALID_SOCKET (-1)
typedef int SOCKET;
#define CNET_SOCK_ERR errno
#endif

#include "util.h"

#define CNET_RECV_BUF_SIZE (1024 * 32)

namespace CNet {

struct DataBuffer {
	char* data;
	U32 len;
	U32 cap;
};

enum ToClientType {
	TO_CLIENT_JOIN_ACCEPT,
	TO_CLIENT_PLAYER_READY,
	TO_CLIENT_SONG_START,
	TO_CLIENT_SPAWN_WALKING_GULL,
	TO_CLIENT_SPAWN_SHOOTING_GULL,
	TO_CLIENT_GULL_EXPLODE
};

enum ToServerType {
	TO_SERVER_JOIN,
	TO_SERVER_READY,
	TO_SERVER_NOTE_INPUT
};

struct alignas(16) MsgHeader {
	U32 msgType;
	U32 msgLength;
};

struct Connection {
	char recvData[CNET_RECV_BUF_SIZE];
	DataBuffer recvBuffer;
	SOCKET socket;
};

bool send_message(SOCKET s, const void* data, U32 dataLen) {
	U32 alignAmount = ALIGN_HIGH(dataLen, 16) - dataLen;
	const char* cdata = (const char*)data;
	// This will hopefully not loop in practice
	while (dataLen) {
		int result = send(s, cdata, dataLen, 0);
		if (result <= 0) {
			int err = CNET_SOCK_ERR();
			if (err != WSAEWOULDBLOCK && err != EAGAIN) {
				return false;
			}
		}
		cdata += result, dataLen -= result;
	}
	// Ensure that the packets are always aligned to 16 so that we can read them directly without provable undefined behavior
	// This loop is just a failsafe, all Msg structs should already be aligned to 16
	char alignmentBuffer[16]{};
	while (alignAmount) {
		int result = send(s, alignmentBuffer, alignAmount, 0);
		if (result <= 0) {
			int err = CNET_SOCK_ERR();
			if (err != WSAEWOULDBLOCK && err != EAGAIN) {
				return false;
			}
		}
		alignAmount -= result;
	}
	return true;
}

bool handle_message_server(Connection* from, MsgHeader* msg) {
	switch (msg->msgType) {
	case TO_SERVER_JOIN: {
		printf("join\n");
	} break;
	case TO_SERVER_READY: {
		printf("ready\n");
	} break;
	case TO_SERVER_NOTE_INPUT: {
		printf("note input\n");
	} break;
	default: return false;
	}
	return true;
}

bool handle_message_client(MsgHeader* msg) {
	switch (msg->msgType) {
	case TO_CLIENT_JOIN_ACCEPT: {
		printf("join accept\n");
	} break;
	case TO_CLIENT_PLAYER_READY: {
		printf("player ready\n");
	} break;
	case TO_CLIENT_SONG_START: {
		printf("song start\n");
	} break;
	case TO_CLIENT_SPAWN_WALKING_GULL: {
		printf("spawn walking gull\n");
	} break;
	case TO_CLIENT_SPAWN_SHOOTING_GULL: {
		printf("spawn shooting gull\n");
	} break;
	case TO_CLIENT_GULL_EXPLODE: {
		printf("gull explode\n");
	} break;
	default: return false;
	}
	return true;
}

bool receive_messages(SOCKET s, DataBuffer& buffer, Connection* recvFromClient) {
	int received = recv(s, buffer.data + buffer.len, buffer.cap - buffer.len, 0);
	if (received < 0) {
		int err = CNET_SOCK_ERR();
		if (err == WSAEWOULDBLOCK || err == EAGAIN) {
			return true;
		}
		return false;
	}
	if (received == 0) {
		return false;
	}
	buffer.len += received;
	U32 offset = 0;
	while (offset + sizeof(MsgHeader) <= buffer.len) {
		MsgHeader* header = (MsgHeader*)(buffer.data + offset);
		if (offset + ALIGN_HIGH(header->msgLength, 16) > buffer.len) {
			break;
		}
		bool handleResult;
		if (recvFromClient) {
			handle_message_server(recvFromClient, header);
		} else {
			handle_message_client(header);
		}
		offset += ALIGN_HIGH(header->msgLength, 16);
	}
	if (offset == 0 && buffer.len == buffer.cap) {
		return false;
	}
	memmove(buffer.data, buffer.data + offset, buffer.len - offset);
	buffer.len -= offset;
	return true;
}

void client_disconnect(Connection* connection) {
	close(connection->socket);
	free(connection);
}

Connection* client_connect_to(const char* ip, const char* port) {
#ifdef _WIN32
	WSAData wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		printf("WSAStartup failed\n");
		return nullptr;
	}
#endif
	struct addrinfo* addrInfoResult;
	struct addrinfo hints = { 0 };
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	if (getaddrinfo(ip, port, &hints, &addrInfoResult) != 0) {
		printf("getaddrinfo failed\n");
		return nullptr;
	}
	SOCKET server = socket(addrInfoResult->ai_family, addrInfoResult->ai_socktype, addrInfoResult->ai_protocol);
	if (server == INVALID_SOCKET) {
		printf("socket failed\n");
		return nullptr;
	}
	if (connect(server, addrInfoResult->ai_addr, addrInfoResult->ai_addrlen) != 0) {
		printf("connect failed\n");
		return nullptr;
	}
	freeaddrinfo(addrInfoResult);
	u_long trueFlagLong;
	if (ioctl(server, FIONBIO, &trueFlagLong) != 0) {
		printf("ioctl failed\n");
		return nullptr;
	}
	int trueFlagInt = 1;
	if (setsockopt(server, IPPROTO_TCP, TCP_NODELAY, (char*)&trueFlagInt, sizeof(int)) != 0) {
		printf("setsockopt failed\n");
		return nullptr;
	}
	Connection* connection = (Connection*)malloc(sizeof(Connection));
	connection->socket = server;
	connection->recvBuffer.data = connection->recvData;
	connection->recvBuffer.len = 0;
	connection->recvBuffer.cap = sizeof(connection->recvData);
	return connection;
}

bool do_client_networking(Connection* server) {
	return receive_messages(server->socket, server->recvBuffer, nullptr);
}

bool do_network_server(const char* port) {
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
	addrinfo hints{};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	addrinfo* service;
	if (getaddrinfo(nullptr, port, &hints, &service) != 0) {
		printf("getaddrinfo failed\n");
		return false;
	}
	if (bind(s, service->ai_addr, (int)service->ai_addrlen) != 0) {
		printf("bind failed\n");
		return false;
	}
	freeaddrinfo(service);
	if (listen(s, SOMAXCONN) != 0) {
		printf("listen failed\n");
		return false;
	}

	struct sockaddr_in sockAddr;
	socklen_t addrLen = sizeof(sockAddr);
	U32 clientCount = 0;
	const U32 MAX_CLIENTS = 16;
	Connection* clients[MAX_CLIENTS];

	while (true) {
		SOCKET newSocket = accept(s, (sockaddr*)&sockAddr, &addrLen);
		if (newSocket != INVALID_SOCKET) {
			if (clientCount != MAX_CLIENTS) {
				Connection* newClient = (Connection*)malloc(sizeof(Connection));
				newClient->socket = newSocket;
				newClient->recvBuffer.data = newClient->recvData;
				newClient->recvBuffer.len = 0;
				newClient->recvBuffer.cap = sizeof(newClient->recvData);
				clients[clientCount++] = newClient;
				MsgHeader testMsg{ TO_CLIENT_JOIN_ACCEPT, sizeof(MsgHeader) };
				if (!send_message(newClient->socket, &testMsg, testMsg.msgLength)) {
					close(newClient->socket);
					free(newClient);
					clientCount--;
				}
			}
		}
		for (U32 i = 0; i < clientCount; i++) {
			Connection* client = clients[i];
			if (!receive_messages(client->socket, client->recvBuffer, client)) {
				printf("Close client\n");
				close(client->socket);
				clients[i--] = clients[--clientCount];
				continue;
			}
		}
		Sleep(10);
	}
	return true;
}

}