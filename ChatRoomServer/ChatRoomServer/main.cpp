#include <arpa/inet.h>
#include <cstdio>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <fstream>
#include <iostream>
#include <netinet/tcp.h>
#include <filesystem>
#include <dirent.h>
#include <vector>

#define MAX_CLIENT 256
#define MAX_PATH 256
#define SIGNAL_SIZE 13

const char* uploadFileSignal = "[uploadstart]";
const char* uploadFileBeginSignal = "[uploadbegin]";
const char* uploadFileFinishSignal = "[uploadfinis]";
const char* getFileListSignal = "[requestfile]";
const char* downloadFileSignal = "[downloadfil]";
char fileDir[256] = { 0 };
char filePath[256] = { 0 };

int clientMsgArray[MAX_CLIENT] = { -1 };
int clientFileArray[MAX_CLIENT] = { -1 };
int clientMsgCount = 0, clientFileCount;
char writingPath[MAX_PATH] = { 0 };

typedef unsigned int uint;
std::vector<std::pair<FILE*, uint>> uploadQueue;
//FILE* file;

void sendMsg(const char* msg, int msgLen, int self) {
	// 将收到的信息转发给除发送者以外的所有在线用户
	sockaddr_in selfAddr;
	socklen_t selfSize = sizeof(selfAddr);
	getsockname(self, (sockaddr*)&selfAddr, &selfSize);

	// 获取当前时间
	time_t timep;
	time(&timep);
	char* time = ctime(&timep);

	// 粘包
	char* ip = inet_ntoa(selfAddr.sin_addr);
	// [ip][time][文字]  
	char ipTimeMsg[BUFSIZ + 16 + 32 + 2 * 3] = { 0 };
	ipTimeMsg[0] = '[';
	memcpy(ipTimeMsg + 1, ip, strlen(ip));
	memset(ipTimeMsg + 1 + strlen(ip), ']', 1);
	memset(ipTimeMsg + 1 + strlen(ip) + 1, '[', 1);
	memcpy(ipTimeMsg + 1 + strlen(ip) + 1 + 1, time, strlen(time) - 1);
	memset(ipTimeMsg + 1 + strlen(ip) + 1 + 1 + strlen(time) - 1, ']', 1);
	memset(ipTimeMsg + 1 + strlen(ip) + 1 + 1 + strlen(time) - 1 + 1, '[', 1);
	memcpy(ipTimeMsg + 1 + strlen(ip) + 1 + 1 + strlen(time) - 1 + 1 + 1, msg, msgLen);
	memset(ipTimeMsg + 1 + strlen(ip) + 1 + 1 + strlen(time) - 1 + 1 + 1 + msgLen, ']', 1);
	printf("send to all: %s\n", ipTimeMsg);

	for (int i = 0; i < clientMsgCount; ++i) {
		if (clientMsgArray[i] != self && clientMsgArray[i] != -1) {
			write(clientMsgArray[i], ipTimeMsg, strlen(ip) + strlen(time) + msgLen + 5);
		}
	}
}

void errorHandling(const char* msg) {
	fputs(msg, stderr);
	printf("\nerror number: %d, %s", errno, strerror(errno));
	fputc('\n', stderr);
	exit(1);
}

void tranverseDir(const char* dirPath, char* fileNames) {
	DIR* pDir;
	struct dirent* ptr;
	if (!(pDir = opendir(dirPath))) {
		errorHandling("dir path do not exist");
	}
	while ((ptr = readdir(pDir)) != 0) {
		if (strcmp(ptr->d_name, ".") != 0 && strcmp(ptr->d_name, "..") != 0) {
			memcpy(fileNames + strlen(fileNames), ptr->d_name, strlen(ptr->d_name));
			fileNames[strlen(fileNames)] = ',';
		}
	}
	closedir(pDir);
}

int main(int argc, char* argv[])
{
	if (argc != 3) {
		fputs("请输入两个端口号！\n使用方法：./可执行文件名 端口1 端口2\n如：./ChatRoomServer 8888 8887\n", stdout);
		exit(0);
	}

	int msgPort = atoi(argv[1]);
	int filePort = atoi(argv[2]);
	printf("端口1：%d, 端口2：%d\n", msgPort, filePort);

	getcwd(fileDir, 256);
	char temp[256] = "/files/";
	memcpy(fileDir + strlen(fileDir), temp, strlen(temp));
	memcpy(filePath, fileDir, strlen(fileDir));

	printf("This is a epoll IO reuse server\n");

	char buffer[1024] = "";
	char fileNames[1024] = "";
	int serverMsgSock, serverFileSock, clientSock;
	sockaddr_in serverMsgAddr, serverFileAddr, clientAddr;
	socklen_t clientSize = sizeof(clientAddr);
	serverMsgSock = socket(PF_INET, SOCK_STREAM, 0);
	serverFileSock = socket(PF_INET, SOCK_STREAM, 0);
	int opt_val = 1;
	setsockopt(serverFileSock, IPPROTO_TCP, TCP_NODELAY, (void*)opt_val, sizeof(opt_val));

	memset(&serverMsgAddr, 0, sizeof(serverMsgAddr));
	serverMsgAddr.sin_family = AF_INET;
	serverMsgAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverMsgAddr.sin_port = htons(msgPort);

	memset(&serverFileAddr, 0, sizeof(serverFileAddr));
	serverFileAddr.sin_family = AF_INET;
	serverFileAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverFileAddr.sin_port = htons(filePort);

	int ret = 0;
	ret = bind(serverMsgSock, (sockaddr*)&serverMsgAddr, sizeof(serverMsgAddr));
	if (ret == -1) errorHandling("serverMsgSock bind error: ");
	ret = bind(serverFileSock, (sockaddr*)&serverFileAddr, sizeof(serverFileAddr));
	if (ret == -1) errorHandling("serverFileSock bind error: ");

	ret = listen(serverMsgSock, 5);
	if (ret == -1) errorHandling("serverMsgSock listen error: ");
	ret = listen(serverFileSock, 5);
	if (ret == -1) errorHandling("serverFileSock listen error: ");

	epoll_event event;
	int epfd, event_count;
	epfd = epoll_create(2048); //里面数字填多少都无所谓, 大于0即可
	if (epfd == -1) errorHandling("epoll create error: ");
	epoll_event* all_events = new epoll_event[100]; //不能超过1024

	event.events = EPOLLIN;
	event.data.fd = serverMsgSock;
	epoll_ctl(epfd, EPOLL_CTL_ADD, serverMsgSock, &event);

	event.events = EPOLLIN;
	event.data.fd = serverFileSock;
	epoll_ctl(epfd, EPOLL_CTL_ADD, serverFileSock, &event);

	while (true) {
		event_count = epoll_wait(epfd, all_events, 100, 1000);
		if (event_count == -1) errorHandling("epoll wait error: ");
		if (event_count == 0) continue;

		for (int i = 0; i < event_count; ++i) {
			int tempfd = all_events[i].data.fd;
			if (tempfd == serverMsgSock) {
				memset(&clientAddr, 0, sizeof(clientAddr));
				clientSock = accept(serverMsgSock, (sockaddr*)&clientAddr, &clientSize);
				if (clientSock == -1) errorHandling("accept error: ");
				// 将连接的客户端加入客户端数组
				clientMsgArray[clientMsgCount] = clientSock;
				++clientMsgCount;

				event.events = EPOLLIN;
				event.data.fd = clientSock;
				epoll_ctl(epfd, EPOLL_CTL_ADD, clientSock, &event);
				printf("msg socket connected: [file descriptor: %d, Ipv4 address: %s]\n", clientSock, inet_ntoa(clientAddr.sin_addr));
			}
			else if (tempfd == serverFileSock) {
				memset(&clientAddr, 0, sizeof(clientAddr));
				clientSock = accept(serverFileSock, (sockaddr*)&clientAddr, &clientSize);
				if (clientSock == -1) errorHandling("accept error: ");
				clientFileArray[clientFileCount] = clientSock;
				++clientFileCount;

				event.events = EPOLLIN;
				event.data.fd = clientSock;
				epoll_ctl(epfd, EPOLL_CTL_ADD, clientSock, &event);
				printf("file socket connected: [file descriptor: %d, Ipv4 address: %s]\n", clientSock, inet_ntoa(clientAddr.sin_addr));
			}
			else {
				sockaddr_in peerAddr;
				socklen_t peerSize = sizeof(peerAddr);
				memset(&peerAddr, 0, sizeof(peerAddr));
				memset(&clientAddr, 0, sizeof(clientAddr));
				getsockname(tempfd, (sockaddr*)&clientAddr, &clientSize);
				getpeername(tempfd, (sockaddr*)&peerAddr, &peerSize);
				//printf("clientAddr.sin_port = %d\n", ntohs(clientAddr.sin_port));
				//printf("peerAddr.sin_port = %d\n", ntohs(peerAddr.sin_port));
				// 如果是发送消息的套接字
				if (ntohs(clientAddr.sin_port) == msgPort) {
					memset(buffer, 0, sizeof(buffer));
					ssize_t len = read(tempfd, buffer, sizeof(buffer));
					printf("from client[%s]: %s\n", inet_ntoa(clientAddr.sin_addr), buffer);
					if (len <= 0) {
						// 将断开连接的客户端移除出客户端数组，将后面的客户端前移一个位置
						for (int j = tempfd; j < clientMsgCount; ++j) {
							if (j < MAX_CLIENT - 1) clientMsgArray[j] = clientMsgArray[j + 1];
							else clientMsgArray[j] = -1;
						}
						--clientMsgCount;
						printf("msg socket disconnect: [file descriptor: %d]\n", tempfd);
						close(tempfd);
						epoll_ctl(epfd, EPOLL_CTL_DEL, tempfd, NULL);
					}
					else {
						sendMsg(buffer, strlen(buffer), tempfd);
					}
				}
				else if (ntohs(clientAddr.sin_port) == filePort) {
					memset(buffer, 0, sizeof(buffer));
					ssize_t len = read(tempfd, buffer, sizeof(buffer));

					char tempStr[15] = "";
					printf("len=%d bool=%d\n", len, len >= SIGNAL_SIZE);
					if (len <= 0) {
						// 将断开连接的客户端移除出客户端数组，将后面的客户端前移一个位置
						for (int j = tempfd; j < clientFileCount; ++j) {
							if (j < MAX_CLIENT - 1) clientFileArray[j] = clientFileArray[j + 1];
							else clientFileArray[j] = -1;
						}
						--clientFileCount;
						printf("file socket disconnect: [file descriptor: %d]\n", tempfd);
						close(tempfd);
						epoll_ctl(epfd, EPOLL_CTL_DEL, tempfd, NULL);
					}
					else {
						if (len >= SIGNAL_SIZE) {
							memset(tempStr, 0, sizeof(tempStr));
							memcpy(tempStr, buffer, SIGNAL_SIZE);
							printf("SIGNAL: %s\n", tempStr);
							// 是发文件开始的信号
							if (strcmp(tempStr, ::uploadFileSignal) == 0) {
								printf("dirpath: [%s]\n", filePath);
								memcpy(filePath + strlen(filePath), buffer + SIGNAL_SIZE, len - SIGNAL_SIZE);
								printf("filepath: [%s]\n", filePath);
								FILE* file = nullptr;
								file = fopen(filePath, "a+");
								if (file == NULL) errorHandling("open failed");
								else printf("isopen\n");
								uploadQueue.push_back(std::make_pair(file, ntohl(peerAddr.sin_addr.s_addr)));
								printf("filepath: [%s] has been add to queue\n", filePath);
								memset(filePath, 0, sizeof(filePath));
								memcpy(filePath, fileDir, strlen(fileDir));
							}
							// 是发文件结束的信号
							else if (strcmp(tempStr, ::uploadFileFinishSignal) == 0) {
								fputs(buffer, stdout);
								int index = 0;
								for (index = 0; index < uploadQueue.size(); ++index) {
									if (uploadQueue[index].second == ntohl(peerAddr.sin_addr.s_addr)) break;
								}
								fclose(uploadQueue[index].first);
								uploadQueue[index].first = nullptr;
								uploadQueue.erase(uploadQueue.begin());
							}
							// 是获取文件列表的信号
							else if (strcmp(tempStr, ::getFileListSignal) == 0) {
								printf("request file list\n");
								memset(fileNames, 0, sizeof(fileNames));
								tranverseDir(::fileDir, fileNames);
								write(tempfd, fileNames, strlen(fileNames));
							}
							// 是下载文件的信号
							else if (strcmp(tempStr, ::downloadFileSignal) == 0) {
								// TODO:写向客户端传文件的代码
								printf("dirpath: [%s]\n", filePath);
								memcpy(filePath + strlen(filePath), buffer + SIGNAL_SIZE, len - SIGNAL_SIZE);
								printf("filepath: [%s]\n", filePath);
								FILE* file = nullptr;
								file = fopen(filePath, "r");
								memset(filePath, 0, sizeof(filePath));
								memcpy(filePath, fileDir, strlen(fileDir));
								if (file == NULL) errorHandling("open failed");
								else printf("isopen\n");
								while (!feof(file)) {
									usleep(100);
									memset(buffer, 0, sizeof(buffer));
									size_t len = fread(buffer, sizeof(char), sizeof(buffer), file);
									fputs(buffer, stdout);
									write(tempfd, buffer, len);
								}
								write(tempfd, ::uploadFileFinishSignal, SIGNAL_SIZE);
								fclose(file);
								printf("donwload files finished\n");
							}
							// 是发过来的文件数据
							else {
								int index = 0;
								fputs(buffer, stdout);
								for (index = 0; index < uploadQueue.size(); ++index) {
									if (uploadQueue[index].second == ntohl(peerAddr.sin_addr.s_addr))
										fputs(buffer, uploadQueue[index].first);
								}
							}
						}
						else {
							int index = 0;
							fputs(buffer, stdout);
							for (index = 0; index < uploadQueue.size(); ++index) {
								if (uploadQueue[index].second == ntohl(peerAddr.sin_addr.s_addr))
									fputs(buffer, uploadQueue[index].first);
							}
						}
					}
				}
			}
		}
	}

	delete[] all_events;
	close(serverMsgSock);
	close(serverFileSock);
	close(epfd);
	return 0;
}