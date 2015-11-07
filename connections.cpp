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

using namespace std;

int UDPsent = 0;
int UDPreceived = 0;

void ipString2Char4(std::string ip, char* buf) // buf must be size 4
{
    ip.replace(ip.find("."),1," ");
    ip.replace(ip.find("."),1," ");
    ip.replace(ip.find("."),1," ");
    
    std::stringstream ssip(ip);
    int a;
    ssip >> a; buf[0] = (char)a; 
    ssip >> a; buf[1] = (char)a; 
    ssip >> a; buf[2] = (char)a; 
    ssip >> a; buf[3] = (char)a; 
}

std::string char42String(char* buf) // buf must be size 4
{
    std::stringstream aux;
    aux << (unsigned int) ((uint8_t)buf[0]) << ".";
    aux << (unsigned int) ((uint8_t)buf[1]) << ".";
    aux << (unsigned int) ((uint8_t)buf[2]) << ".";
    aux << (unsigned int) ((uint8_t)buf[3]);

    return aux.str();
}

//caller need to free the char*
std::string getOwnIPAddr(){
    struct ifaddrs * ifAddrStruct=NULL;
    struct ifaddrs * ifa=NULL;
    void * tmpAddrPtr=NULL;

    getifaddrs(&ifAddrStruct);
    std::string result;

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
    // printf("UDP Listening at port %d...\n", port);

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
    // printf("TCP Listening at port %d...\n", port);

    return listenFd;
}

int listen_socket(int listenFd)
{

    socklen_t len; //store size of the address
    int connFd;
    struct sockaddr_in clntAdd;

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

int robustRead(int connFd, char * buffer, int length){
    int count = 0, add = 0;
    while(count < length){
        if( (add = read(connFd, buffer, length)) > 0 ){
            count += add;
            buffer += add;
        }
        else if( add < 0 ){
            return -10;
        }
        else if(add == 0){
            break;
        }
    }
    return count;
}

int robustWrite(int connFd, char * buffer, int length){
    int count = 0, add = 0;
    while(count < length){
        if( ( add = write(connFd, buffer, length)) > 0 ){
            count += add;
            buffer += add;
        }
        else if( add < 0 ){
            return -1;
        }
    }
    return count;
}

//write large package on socket
size_t splitWrite( int connFd, char * data, size_t size ){
    
    int msg_num = size / BUFFER_MAX + 1;
    int rest = size % ((int)BUFFER_MAX);
    
    //store the should-be message sizes
    message *msgs = new message[msg_num];
    for(int i=1; i < msg_num; i++){
        msgs[i].begin = msgs[i-1].begin + BUFFER_MAX;
    }
    msgs[msg_num-1].length = rest;

    char writeSuccess = 0;
    for(int i=0; i < msg_num; i++){
        //write each message, max size is BUFFER_MAX
        robustWrite( connFd, data + msgs[i].begin, msgs[i].length );
        
        //cout<<"Server: sent " << msgs[i].length << endl;
        //usleep(200*1000);
        
        //make sure client do got the data
        robustRead( connFd, &writeSuccess, 1 );
        
        //cout<<"Server: recv "<<(int)writeSuccess<<endl;
        //usleep(200*1000);

        //if package failed, send again
        if(writeSuccess==0) i--;
    }
    return size;
}

size_t splitRead( int connFd, char * data, size_t size ){
    
    int msg_num = size / BUFFER_MAX + 1;
    int rest = size % ((int)BUFFER_MAX);
    
    //store the should-be message sizes
    message *msgs = new message[msg_num];
    for(int i=1; i < msg_num; i++){
        msgs[i].begin = msgs[i-1].begin + BUFFER_MAX;
    }
    msgs[msg_num-1].length = rest;

    char readSuccess = 0;
    int readSize;

    for(int i=0; i < msg_num; i++){

        //read message from socket. max size BUFFER_MAX
        readSize = robustRead( connFd, data + msgs[i].begin, msgs[i].length );
        
        //cout<<"client recv size :" << readSize << endl;
        //usleep(200*1000);

        //check if reading success or not
        if(readSize == msgs[i].length)  readSuccess = 1;
        else    readSuccess = 0;

        //tell server the data transmission status (good or fail)
        robustWrite( connFd, &readSuccess, 1);
        
        //cout<<"Client: sent "<<endl;
        //usleep(200*1000);
        
        //if reading faile, read again
        if(readSuccess==0) i--;
    }
    return size;
}

