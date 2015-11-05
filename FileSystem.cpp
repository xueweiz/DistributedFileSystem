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

//extern Membership * membership;

FileSystem::FileSystem(int port, Membership& m) :
    membership(m), port(port)
{
	logFile.open("logFileSystem.log"); // we can make all the logs going to a single file, but nah.

    std::thread th2(&FileSystem::updateThread, this);
    ringUpdating.swap(th2);

    std::thread th1(&FileSystem::listeningThread, this);
    listening.swap(th1);
}

/*
FileSystem::FileSystem(int port ) : port(port)
{
    logFile.open("logFileSystem.log"); // we can make all the logs going to a single file, but nah.

    std::thread th1(&FileSystem::listeningThread, this);
    listening.swap(th1);
}*/

FileSystem::~FileSystem()
{

}

int FileSystem::findPositionByKey( int key ){
    int position = 0;
    virtualRingLock.lock();
    for(int i=0; i < virtualRing.size(); i++){
        if(virtualRing[i].key >= key ){ //find the first virtual node with n.key >= key. If not, return the first Node.
            position = i;
            break;
        }
    }
    virtualRingLock.unlock();
    return position;
}

bool FileSystem::putToNode(int nodePosition, std::string localFile, std::string remoteFile){
    nodePosition = ( nodePosition + virtualRing.size() ) % virtualRing.size();
    std::string destAddress = virtualRing[nodePosition].ip_str;    
    return put(destAddress, localFile, remoteFile);
}

bool FileSystem::getFromNode(int nodePosition, std::string localFile, std::string remoteFile){
    nodePosition = ( nodePosition + virtualRing.size() ) % virtualRing.size();
    std::string destAddress = virtualRing[nodePosition].ip_str;    
    return get(destAddress, localFile, remoteFile);
}

void FileSystem::put(std::string localFile, std::string remoteFile)
{
    // hashing function to find the machine/s where to store the file;
    
    int fileKey = hashString(localFile);
    int position = findPositionByKey(fileKey);

    cout<<"put "<<localFile<<" "<<fileKey<<" to "<<position<<" "<<virtualRing[position].ip_str<<" "<<virtualRing[position].key<<endl;

    bool attempt = putToNode( position, localFile, remoteFile );
    if(attempt)
        return;
    
    attempt = putToNode( position+1, localFile, remoteFile );
    if(attempt)
        return;
    
    attempt = putToNode( position+2, localFile, remoteFile );
    if(attempt)
        return;


    //std::string destAddress = "localhost";
    //put(destAddress, localFile, remoteFile);
}

void FileSystem::get(std::string localFile, std::string remoteFile){
    //https://gitlab-beta.engr.illinois.edu/remis2/MP3-FileSystem.git
    // hashing function to find the machine where to ask for the file;

    int fileKey = hashString(localFile);
    int position = findPositionByKey(fileKey);

    cout<<"get "<<localFile<<" "<<fileKey<<" from "<<position<<" "<<virtualRing[position].ip_str<<" "<<virtualRing[position].key<<endl;

    bool attempt = getFromNode( position, localFile, remoteFile );
    if(attempt)
        return;
    
    attempt = getFromNode( position+1, localFile, remoteFile );
    if(attempt)
        return;
    
    attempt = getFromNode( position+2, localFile, remoteFile );
    if(attempt)
        return;

    //std::string destAddress = "localhost";
    //get(destAddress, localFile, remoteFile);
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
            msg.size = readFile(filename, &buffer);
            write(connFd, &msg, sizeof(Message_fs));
            write(connFd, buffer, msg.size);
        }

        close(connFd);
        delete buffer;        
    }
    return;
}

//Internal for the file system
void FileSystem::saveFile(std::string filename, char* buffer, size_t length)
{
    std::string root = "files/";
    std::ofstream newfile(root + filename, std::ofstream::binary);

    newfile.write(buffer, length);
    newfile.close();

    logFile << "File " << root + filename << " saved" << std::endl;
}

//Internal for the filesystem
int FileSystem::readFile(std::string filename, char** buffer)
{
    std::string root = "files/";
    std::ifstream file(root + filename, std::ifstream::binary);

    if (!file.good())
    {
        std::cout << "Could not open file: " << filename << std::endl;
        logFile << "Could not open file: " << filename << std::endl;
        return 0;
    }

    file.seekg (0, file.end);
    int length = file.tellg();
    file.seekg (0, file.beg);        

    *buffer = new char [length];

    file.read (*buffer,length);
    file.close();

    logFile << "File " << root + filename << " read" << std::endl;
    std::cout << "File " << root + filename << " read" << std::endl;

    return length;
}

bool FileSystem::put(std::string address, std::string localFile, std::string remoteFile)
{
    int connectionToServer; //TCP connect to introducer/other nodes

    // printf("Local file: %s\n", localFile.c_str());
    // printf("Remote file: %s\n", remoteFile.c_str());

    std::ifstream file(localFile, std::ifstream::binary);

    if (!file.good())
    {
        std::cout << "Could not open file: " << localFile << std::endl;
        logFile << "Could not open file: " << localFile << std::endl;
        return false;
    }

    logFile << "put: Connecting to "<< address << "..." << std::endl;

    int ret = connect_to_server(address.c_str(), port, &connectionToServer);
    if(ret!=0)
    {
        logFile <<"ERROR Put: Cannot connect to "<<address<< std::endl;
        std::cout <<"ERROR Put: Cannot connect to "<<address<< std::endl;
        return false;
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
        msg.size = length;

        write(connectionToServer, &msg, sizeof(Message_fs) );
        write(connectionToServer, buffer, length);     

        close(connectionToServer);
        delete buffer;
    }   

    file.close(); 
    return true;
}

bool FileSystem::get(std::string address, std::string localFile, std::string remoteFile)
{
    int connectionToServer; //TCP connect to introducer/other nodes

    logFile << "put: Connecting to "<< address << "..." << std::endl;

    int ret = connect_to_server(address.c_str(), port, &connectionToServer);
    if(ret!=0)
    {
        logFile <<"ERROR Put: Cannot connect to "<<address<< std::endl;
        std::cout <<"ERROR Put: Cannot connect to "<<address<< std::endl;
        return false;
    }
    else{

        Message_fs msg;
        msg.type = MSG_GET;
        remoteFile.copy(msg.filename, remoteFile.length());

        write(connectionToServer, &msg, sizeof(Message_fs)); // Send filename
        read (connectionToServer, &msg, sizeof(Message_fs)); // Receive size

        char * buffer = new char [msg.size];

        read(connectionToServer, buffer,  msg.size);
        //std::cout << "Received : " << msg.size << std::endl;

        std::ofstream file(localFile, std::ofstream::binary);

        if (!file.good())
        {
            std::cout << "Could not open file: " << localFile << std::endl;
            logFile << "Could not open file: " << localFile << std::endl;
            return false;
        }

        file.write(buffer, msg.size); 
        file.close();  

        close(connectionToServer);
        delete buffer;
    }

    return true;
}


void FileSystem::pull(std::string address, std::string filename)
{

}

void FileSystem::push(std::string address, std::string filename)
{

}

bool FileSystem::VirtualNode::operator<(VirtualNode const & second) const{
    return this->key < second.key ? true : false;
}

bool FileSystem::VirtualNode::operator>(VirtualNode const & second) const{
    return this->key > second.key ? true : false;
}

bool FileSystem::VirtualNode::operator==(VirtualNode const & second) const{
    return this->key == second.key ? true : false;
}

bool FileSystem::pullFileFromRange( int socketFd, int min, int max, bool rmRemoteFile ){
    return false;
}

int FileSystem::hashString( std::string s ){
    unsigned int value = (unsigned int) ( std::hash<std::string>()(s) );
    return ( (int)(value % RING_SIZE) );
}

//add new joining node&hash to virtual ring, update myPosition. return the relative position to new node 
int FileSystem::addToVirtualRing( Node n ){
    VirtualNode newNode;
    newNode.ip_str = n.ip_str;
    newNode.key = hashString(newNode.ip_str);

    virtualRingLock.lock();
    
    virtualRing.push_back(newNode);
    std::sort(virtualRing.begin(), virtualRing.end());
    myPosition = find( virtualRing.begin(), virtualRing.end(), myself ) - virtualRing.begin();
    int joinPosition = find( virtualRing.begin(), virtualRing.end(), newNode ) - virtualRing.begin();

    virtualRingLock.unlock();

    return joinPosition - myPosition;
}

//delete the leaving node&hash to virtual ring, update myPosition. return the relative position to new node 
int FileSystem::deleteFromVirtualRing( Node n ){
    cout<<"detect leave: "<<n.ip_str<<endl;

    VirtualNode changeNode;
    changeNode.ip_str = n.ip_str;
    changeNode.key = hashString(changeNode.ip_str);

    cout<<"leave virtual node: "<<changeNode.ip_str<<" "<<changeNode.key<<endl;

    virtualRingLock.lock();
    
    int myOldPosition = myPosition;

    int leavePosition = find( virtualRing.begin(), virtualRing.end(), changeNode ) - virtualRing.begin();

    cout<<"leave node position: "<<leavePosition<<endl;

    virtualRing.erase( virtualRing.begin() + leavePosition );

    if( leavePosition == myOldPosition )
        myPosition = -1;
    else
        myPosition = find( virtualRing.begin(), virtualRing.end(), myself ) - virtualRing.begin();
    
    virtualRingLock.unlock();

    return leavePosition - myOldPosition;
}

void FileSystem::printVirtualRing(){
    virtualRingLock.lock();
    std::cout<<"myself: "<<myself.ip_str<<" "<<myself.key<<std::endl;
    for(int i=0; i < virtualRing.size(); i++){
        std::cout<<virtualRing[i].ip_str<<" "<<virtualRing[i].key<<std::endl;
    }
    std::cout<<std::endl;
    virtualRingLock.unlock();
}

bool FileSystem::detectJoin( Node joinNode ){
    addToVirtualRing( joinNode );

    return true;
}

bool FileSystem::myselfJoin( Node myNodeToChange ){
    addToVirtualRing( myNodeToChange );

    return true;
}

bool FileSystem::detectLeave( Node leaveNode ){
    
    deleteFromVirtualRing( leaveNode );

    return true;
}

bool FileSystem::myselfLeave( Node myNodeToChange ){
    deleteFromVirtualRing( myNodeToChange );

    return true;
}

void FileSystem::updateThread(){
    myNode = membership.getMyNode();
    myself.ip_str = myNode.ip_str;
    myself.key = hashString( myself.ip_str );

    while(true){
        MemberUpdateMsg msg = membership.pullMsgFromFileSysQueue();
        
        if(msg.type == MSG_JOIN){
            cout<<"recv join msg: "<<msg.node.ip_str<<endl;
            if(msg.node.ip_str.compare(myNode.ip_str) != 0)
                detectJoin( msg.node );
            else
                myselfJoin( msg.node );
        }
        else if(msg.type == MSG_LEAVE || msg.type == MSG_FAIL ){
            cout<<"recv leave msg: "<<msg.node.ip_str<<endl;
            if( msg.node.ip_str.compare(myNode.ip_str) != 0 )
                detectLeave( msg.node );
            else
                myselfLeave( msg.node );
        }
        printVirtualRing();
    }
}
