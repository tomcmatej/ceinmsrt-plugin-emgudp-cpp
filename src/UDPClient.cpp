#include <winsock2.h>
#include <stdio.h>

void InitWinsock()
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
}

int main(int argc, char* argv[])
{
	SOCKET socketC;

	InitWinsock();
	struct sockaddr_in serverInfo;
	int len = sizeof(serverInfo);
	serverInfo.sin_family = AF_INET;
	serverInfo.sin_port = htons(1234);
	serverInfo.sin_addr.s_addr = inet_addr("127.0.0.1");

	socketC = socket(AF_INET, SOCK_DGRAM, 0);
	while (1)
	{
		char buffer[1024];
		ZeroMemory(buffer, sizeof(buffer));
		printf("Please input your message: ");
		scanf("%s", buffer);
		if (strcmp(buffer, "exit") == 0)
			break;
		if (sendto(socketC, buffer, sizeof(buffer), 0, (sockaddr*)&serverInfo, len) != SOCKET_ERROR)
		{
			if (recvfrom(socketC, buffer, sizeof(buffer), 0, (sockaddr*)&serverInfo, &len) != SOCKET_ERROR)
			{
				printf("Receive response from server: %s\n", buffer);
			}
		}
	}
	closesocket(socketC);

	return 0;
}