#include <winsock2.h>
#include <stdio.h>

void InitWinsock()
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
}

int main(int argc, char* argv[])
{
	SOCKET socketS;

	InitWinsock();
	struct sockaddr_in local;
	struct sockaddr_in from;
	int fromlen = sizeof(from);
	local.sin_family = AF_INET;
	local.sin_port = htons(1234);
	local.sin_addr.s_addr = INADDR_ANY;

	socketS = socket(AF_INET, SOCK_DGRAM, 0);
	bind(socketS, (sockaddr*)&local, sizeof(local));
	while (1)
	{
		char buffer[1024];
		ZeroMemory(buffer, sizeof(buffer));
		printf("Waiting...\n");
		if (recvfrom(socketS, buffer, sizeof(buffer), 0, (sockaddr*)&from, &fromlen) != SOCKET_ERROR)
		{
			printf("Received message from %s: %s\n", inet_ntoa(from.sin_addr), buffer);
			sendto(socketS, buffer, sizeof(buffer), 0, (sockaddr*)&from, fromlen);
		}
		Sleep(500);
	}
	closesocket(socketS);

	return 0;
}