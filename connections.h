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

#include "constant.h"

// Helpers
std::string getOwnIPAddr();
std::string getSenderIP(char* carrierAdd);
void ipString2Char4(std::string ip, char* buf);
std::string char42String(char* buf);

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

//message
class message{
public:
    message(){ begin=0; length=BUFFER_MAX; };
    int begin;
    int length;
};


int robustRead(int connFd, char * buffer, int length);

int robustWrite(int connFd, char * buffer, int length);

//write large package on socket
size_t splitWrite( int connFd, char * data, size_t size );

size_t splitRead( int connFd, char * data, size_t size );


#endif