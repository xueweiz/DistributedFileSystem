#include <iostream>
#include <string> 
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "connections.h"
#include "constant.h"

int UDPsent = 0;
int UDPreceived = 0;

int getUDPSent()
{
    return UDPsent;
}

int getUDPReceived()
{
    return UDPreceived;
}

int bindSocket(int port)
{
    struct sockaddr_in svrAdd, clntAdd;
    
    //create socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if(sockfd < 0)
    {
        std::cout << "Cannot open socket" << std::endl;
        exit(1);
    }
    
    memset((char*) &svrAdd,0, sizeof(svrAdd));
    
    svrAdd.sin_family = AF_INET;
    svrAdd.sin_addr.s_addr = INADDR_ANY;
    svrAdd.sin_port = htons(port);
    
    //bind socket
    int ret = ::bind(sockfd, (struct sockaddr *)&svrAdd, sizeof(svrAdd));
    if( ret < 0)
    {
        std::cout << "open_socket: Cannot bind" << std::endl;
        exit(1);
    }
    printf("UDP Listening at port %d...\n", port);

    return sockfd;
}

int receiveUDP(int sockfd, char* buf, uint32_t len, std::string& sender)
{
    struct sockaddr addr;
    socklen_t fromlen = sizeof addr;

    int byte_count = recvfrom(sockfd, buf, len, 0, &addr, &fromlen);

    if (byte_count == -1)
    {
        printf("ERROR RECEIVING!!!\n");
        exit(-1);
    }

    struct sockaddr_in *sin = (struct sockaddr_in *) &addr;

    sender = inet_ntoa(sin->sin_addr);

    UDPreceived++;

    return byte_count;
}

void sendUDP(int sockfd, std::string& add, int port, char* buf, uint32_t len)
{
    struct sockaddr_in servaddr,cliaddr;
    struct hostent *server;

    server = gethostbyname(add.c_str());

    if(server == NULL)
    {
        std::cout << "Host does not exist" << std::endl;
        exit(1);
    }

    memset((char *) &servaddr,0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    memcpy((char *) &servaddr.sin_addr.s_addr,(char *) server -> h_addr, server -> h_length);
    servaddr.sin_port = htons(port);
/*
    //int random = rand() % 100;
    //if ((random) <= 10 ) return; 

    static int counter = 1;
    if ((counter++) % 4 == 0 ){
        //std::cout << "Drop message: " << (unsigned int) (((Message* )(buf))->type) << " " << add << std::endl;
        return; 
    } 
*/

    int ret = sendto(sockfd,buf, len, 0, (struct sockaddr *)&servaddr,sizeof(servaddr));

    if (ret == -1)
    {
        printf("Error in sendUDP: cannot send\n");
        return;
    }
    UDPsent++;
    //printf("Message sent\n");
}


/* Server operation */
int open_socket(int port)
{
    struct sockaddr_in svrAdd, clntAdd;
    int listenFd;
    
    //create socket
    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;

    if (setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) 
    {
        printf("setsockopt\n");
        exit(1);
    }
    
    if(listenFd < 0)
    {
        std::cout << "Cannot open socket" << std::endl;
        exit(1);
    }
    
    memset((char*) &svrAdd,0, sizeof(svrAdd));
    
    svrAdd.sin_family = AF_INET;
    svrAdd.sin_addr.s_addr = INADDR_ANY;
    svrAdd.sin_port = htons(port);
    
    //bind socket

    int ret = ::bind(listenFd, (struct sockaddr *)&svrAdd, sizeof(svrAdd));
    if(ret < 0)
    {
        std::cout << "open_socket: Cannot bind" << std::endl;
        exit(1);
    }
    printf("TCP Listening at port %d...\n", port);

    return listenFd;
}

int listen_socket(int listenFd)
{

    socklen_t len; //store size of the address
    int connFd;
    struct sockaddr_in svrAdd, clntAdd;

    listen(listenFd, 5);
    
    len = sizeof(clntAdd);

    //this is where client connects. svr will hang in this mode until client conn
    connFd = accept(listenFd, (struct sockaddr *)&clntAdd, &len);

    if (connFd < 0)
    {
        std::cout << "Cannot accept connection" << std::endl;
        exit(1);
    }
    else
    {
        //std::cout << "Server Connection successful " << std::endl;
    }

    return connFd;
}

/*Client operation */
int connect_to_server(const char* add, int port, int* connectionFd)
{
    struct sockaddr_in svrAdd;
    struct hostent *server;
    
    if((port > 65535) || (port < 2000))
    {
        std::cout<<"Please enter port number between 2000 - 65535"<<std::endl;
        exit(1);
    }       
    
    //create client skt
    *connectionFd = socket(AF_INET, SOCK_STREAM, 0);
        int iSetOption = 1;
    setsockopt(*connectionFd, SOL_SOCKET, SO_REUSEADDR, (char*)&iSetOption,
        sizeof(iSetOption));
    
    if(*connectionFd < 0)
    {
        std::cout << "Cannot open socket" << std::endl;
        exit(1);
    }
    else{
        //std::cout << "Socket opened" << std::endl;
    }
    
    server = gethostbyname(add);
    
    if(server == NULL)
    {
        std::cout << "Host does not exist" << std::endl;
        exit(1);
    }
    
    memset((char *) &svrAdd,0, sizeof(svrAdd));
    svrAdd.sin_family = AF_INET;
    
    memcpy((char *) &svrAdd.sin_addr.s_addr,(char *) server -> h_addr, server -> h_length);
    
    svrAdd.sin_port = htons(port);
    
    int checker = connect(*connectionFd,(struct sockaddr *) &svrAdd, sizeof(svrAdd));
    
    if (checker < 0)
    {
        //printf("Cannot connect to: %s \n",add);
        return -1;
        //exit(1);
    }
    else{
        return 0;
        //std::cout << "Client Connection Successful" << std::endl;
    }
}

//caller need to free the char*
string getOwnIPAddr(){
    struct ifaddrs * ifAddrStruct=NULL;
    struct ifaddrs * ifa=NULL;
    void * tmpAddrPtr=NULL;

    getifaddrs(&ifAddrStruct);
    string result;

    int i=0;
    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }
        i++;
        if(i==4){
            tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            result = addressBuffer;
        }
    }
    
    if (ifAddrStruct!=NULL) freeifaddrs(ifAddrStruct);
    return result;
}

std::string getSenderIP(char* carrierAdd){
    std::stringstream ip_ss;

    ip_ss << (int)(uint8_t)carrierAdd[0] << ".";
    ip_ss << (int)(uint8_t)carrierAdd[1] << ".";
    ip_ss << (int)(uint8_t)carrierAdd[2] << ".";
    ip_ss << (int)(uint8_t)carrierAdd[3];

    std::string ip_carrier = ip_ss.str();

    return ip_carrier;
}