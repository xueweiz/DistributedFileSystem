#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#include <sys/types.h>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string> 
#include <string.h> 
#include <ifaddrs.h>
#include <unistd.h>

using namespace std;

//UDP
int bindSocket(int port);

int receiveUDP(int sockfd, char* buf, uint32_t len, std::string& sender);

void sendUDP(int sockfd, std::string& add, int port, char* buf, uint32_t len);
int getUDPSent();
int getUDPReceived();

//TCP
int open_socket(int port);

int listen_socket(int listenFd);

int connect_to_server(const char* add, int port, int* connectionFd);

string getOwnIPAddr();

string getSenderIP(char* carrierAdd);

#endif