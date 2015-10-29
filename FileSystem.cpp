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

#define BUFFER_FILE_SIZE 1024*1024;

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

        Message_fs msg;

        ret = read(connFd, &msg, sizeof(Message_fs));
        std::string filename = msg.filename;
        char* buffer;

        if(msg.type == MSG_PUT)
        {
            buffer = new char[msg.size];
            ret = read(connFd, buffer, msg.size);
            saveFile(filename, buffer, msg.size);
        }
        if(msg.type == MSG_GET)
        {

        }

        close(connFd);
        delete buffer;        
    }
    return;
}

void FileSystem::saveFile(std::string filename, char* buffer, size_t length)
{
    std::string root = "files/";
    std::ofstream newfile(root + filename, std::ofstream::binary);

    newfile.write(buffer, length);
    newfile.close();

    logFile << "File " << root + filename << " saved" << std::endl;
}


void FileSystem::put(std::string address, std::string localFile, std::string remoteFile)
{
    int connectionToServer; //TCP connect to introducer/other nodes

    // printf("Local file: %s\n", localFile.c_str());
    // printf("Remote file: %s\n", remoteFile.c_str());

    std::ifstream file(localFile, std::ifstream::binary);

    if (!file.good())
    {
        std::cout << "Could not open file: " << localFile << std::endl;
        logFile << "Could not open file: " << localFile << std::endl;
        return;
    }

    logFile << "put: Connecting to "<< address << "..." << std::endl;

    int ret = connect_to_server(address.c_str(), port, &connectionToServer);
    if(ret!=0)
    {
        logFile <<"ERROR Put: Cannot connect to "<<address<< std::endl;
        std::cout <<"ERROR Put: Cannot connect to "<<address<< std::endl;
        return;
    }
    else{

        // Read the file
        file.seekg (0, file.end);
        int length = file.tellg();
        file.seekg (0, file.beg);        

        char * buffer = new char [length];

        file.read (buffer,length);

        Message_fs msg;
        msg.type = MSG_PUT;
        remoteFile.copy(msg.filename, remoteFile.length());
        //msg.filename[remoteFile.length()+1]='\0';
        msg.size = length;

        int ret = write(connectionToServer, &msg, sizeof(Message_fs) );

        write(connectionToServer, buffer, length);     

        close(connectionToServer);
        delete buffer;
    }    
}

void FileSystem::put(std::string localFile, std::string remoteFile)
{
    // hashing function to find the machine where to store the file;

    std::string destAddress = "localhost";
    put(destAddress, localFile, remoteFile);
}

void FileSystem::get(std::string address, std::string localFile, std::string remoteFile)
{

}

void FileSystem::pull(std::string address, std::string filename)
{

}

void FileSystem::push(std::string address, std::string filename)
{

}