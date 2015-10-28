#include <netinet/in.h> 
#include <string.h> 
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <iomanip>
#include <stdlib.h>     // atoi
#include <unistd.h>     // sleep
#include <stdio.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "connections.h"
#include "constant.h"
#include "ChronoCpu.h"
#include "FileSystem.h"

FileSystem::FileSystem(int port, Membership& m) :
    membership(m), port(port)
{
	logFile.open("logFileSystem.log"); // we can make all the logs going to a single file, but nah.

    std::thread th1(&FileSystem::listeningThread, this);
    listening.swap(th1);

}

FileSystem::~FileSystem()
{

}

void FileSystem::listeningThread()
{ 
    int listenFd = open_socket(port); 

    while(true)
    {
        int ret;
        int connFd = listen_socket(listenFd);

        logFile<<"listeningThread: someone is contacting me... "<< std::endl;

        char a = '6';

        read(connFd, &a, sizeof(char));

        if (a == '4')
        {
            a = '6';
            write(connFd, &a, sizeof(char));
            std::cout << "Send correctly" << std::endl;
        }
        

        close(connFd);        
    }
    return;
}



void FileSystem::put(std::string address, std::string filename)
{
    int connectionToServer;
    //TCP connect to introducer/other nodes
    std::cout<<"put: Connecting to "<< address << "..." << std::endl;

    int ret = connect_to_server(address.c_str(), port, &connectionToServer);
    if(ret!=0)
    {
        logFile <<"ERROR Put: Cannot connect to "<<address<< std::endl;
        std::cout <<"ERROR Put: Cannot connect to "<<address<< std::endl;
        return;
    }
    else{

        char a = '4';
        int ret = write(connectionToServer, &a, sizeof(char) );
        
        read(connectionToServer, &a, sizeof(char));

        if (a == '6')
        {
            std::cout << "Received correctly" << std::endl;
        }        

        close(connectionToServer);
    }    
}

void FileSystem::get(std::string address, std::string filename)
{

}

void FileSystem::pull(std::string address, std::string filename)
{

}

void FileSystem::push(std::string address, std::string filename)
{

}