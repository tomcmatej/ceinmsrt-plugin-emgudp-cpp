#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <iostream>
#include <csignal>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

bool stop;

void CEINMSSigintHandler(int sig)
{
	stop = true;
}


int main(void)
{
	stop = false;
	signal(SIGINT, CEINMSSigintHandler);
	SOCKET socketS;

	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	struct sockaddr_in local;
	struct sockaddr_in from;
	int fromlen = sizeof(from);
	local.sin_family = AF_INET;
	local.sin_port = htons(1234);
	local.sin_addr.s_addr = INADDR_ANY;

	socketS = socket(AF_INET, SOCK_DGRAM, 0);
	bind(socketS, (sockaddr*)&local, sizeof(local));

	getsockname(socketS, (SOCKADDR *)&local, (int *)sizeof(local));
	printf("Server: Receiving IP(s) used: %s\n", inet_ntoa(local.sin_addr));
	printf("Server: Receiving port used: %d\n", htons(local.sin_port));
	printf("Server: I\'m ready to receive a datagram...\n");
	
	std::vector<double> data;
	data.push_back(0.1);
	data.push_back(0.2);
	data.push_back(0.3);
	data.push_back(0.4);
	std::vector<char> dataSend;
	//dataSend.resize(2);
	dataSend.resize(sizeof(double)*data.size());
	memcpy(dataSend.data(), data.data(), sizeof(double)*data.size());

	std::cout << dataSend.size() << std::endl;

	char ReceiveBuf[1024];
	ZeroMemory(ReceiveBuf, sizeof(ReceiveBuf));
	

	if (recvfrom(socketS, ReceiveBuf, 1, 0, (sockaddr*)&from, &fromlen) == SOCKET_ERROR)
	{
		closesocket(socketS);
		WSACleanup();
		return 0;
	}


	std::cout << "Number of Channel needed: " << int(ReceiveBuf[0]) << std::endl;
	if (int(ReceiveBuf[0]) != 4)
	{
		std::cout << "ERROR: Number of Channel needed different from 4." << std::endl;
		closesocket(socketS);
		WSACleanup();
		return 0;
	}
	std::cout << "data is being send..." << std::endl;

	while (!stop)
	{
		sendto(socketS, dataSend.data(), dataSend.size(), 0, (sockaddr*)&from, fromlen);
		Sleep(10);
	}
	closesocket(socketS);
	WSACleanup();

	return 0;
}



