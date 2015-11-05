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


int hashString( std::string s ){
    unsigned int value = (unsigned int) ( std::hash<std::string>()(s) );
    return ( (int)(value % RING_SIZE) );
}


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
    return putToAddress(destAddress, localFile, remoteFile);
}

bool FileSystem::getFromNode(int nodePosition, std::string localFile, std::string remoteFile){
    nodePosition = ( nodePosition + virtualRing.size() ) % virtualRing.size();
    std::string destAddress = virtualRing[nodePosition].ip_str;    
    return getFromAddress(destAddress, localFile, remoteFile);
}

void FileSystem::put(std::string localFile, std::string remoteFile)
{
    // hashing function to find the machine/s where to store the file;
    
    int fileKey = hashString(localFile);
    int position = findPositionByKey(fileKey);

    cout<<"put "<<localFile<<" "<<fileKey<<" to "<<position<<" "<<virtualRing[position].ip_str<<" "<<virtualRing[position].key<<endl;

    bool attempt = putToNode( position, localFile, remoteFile );
    //if(attempt)
        //return;
    
    attempt = putToNode( position+1, localFile, remoteFile );
    //if(attempt)
        //return;
    
    attempt = putToNode( position+2, localFile, remoteFile );
    //if(attempt)
        //return;


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
            delete buffer;
        }
        if(msg.type == MSG_GET)
        {
            msg.size = readFile(filename, &buffer);
            write(connFd, &msg, sizeof(Message_fs));
            write(connFd, buffer, msg.size);
            delete buffer;
        }
        if(msg.type == MSG_PULL_CP || msg.type == MSG_PULL_MV){
            sendFileByRange(connFd, msg);
        }

        close(connFd);
    }
    return;
}

bool inRange(int key, int min, int max){
    if(min < max)
        return (min<key && key<=max);
    else
        return ( (0<=key <= min) || (max<key<=RING_SIZE)  );
}

bool FileSystem::sendFileByRange(int connFd, Message_fs msg ){
    bool deleteMyCopy = ( (msg.type == MSG_PULL_MV) ? true : false );

    filesLock.lock();
    size_t count = 0;
    for( auto &file : files ){
        if( inRange(file.key, msg.minKey, msg.maxKey) )
            count++;
    }
    msg.size = count;
    write(connFd, &msg, sizeof(Message_fs));
    for( auto &file : files ){
        if( inRange(file.key, msg.minKey, msg.maxKey) ){
            file.filename.copy(msg.filename, file.filename.length());

            char * buffer; 
            msg.size = readFile(file.filename, &buffer);

            write(connFd, &msg, sizeof(Message_fs));
            write(connFd, buffer, msg.size);

            delete buffer;

            if(deleteMyCopy){
                files.remove( file );   //maintain file list
                deleteFile( msg.filename );
            }
        }
    }

    filesLock.unlock();
    return true;
}

//does not maintain file list!
bool FileSystem::deleteFile( std::string  filename ){
    std::string command = "rm files/" + filename;
    return true;
}

//Internal for the file system
bool FileSystem::saveFile(std::string filename, char* buffer, size_t length)
{
    std::string root = "files/";
    std::ofstream newfile(root + filename, std::ofstream::binary);

    newfile.write(buffer, length);
    newfile.close();

    FileEntry fileEntry(filename);

    filesLock.lock();
    files.push_back( fileEntry );
    filesLock.unlock();

    logFile << "File " << root + filename << " saved" << std::endl;

    return true;
}

//Internal for the filesystem
int FileSystem::readFile(std::string filename, char** buffer)
{
    std::string root = "files/";
    std::ifstream file(root + filename, std::ifstream::binary);

    if (!file.good())
    {
        std::cout << "readFile: Could not open file: " << filename << std::endl;
        logFile << "readFile: Could not open file: " << filename << std::endl;
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

bool FileSystem::putToAddress(std::string address, std::string localFile, std::string remoteFile)
{
    int connectionToServer; //TCP connect to introducer/other nodes

    // printf("Local file: %s\n", localFile.c_str());
    // printf("Remote file: %s\n", remoteFile.c_str());

    std::ifstream file(localFile, std::ifstream::binary);

    if (!file.good())
    {
        std::cout << "putToAddress: Could not open file: " << localFile << std::endl;
        logFile << "putToAddress: Could not open file: " << localFile << std::endl;
        return false;
    }

    logFile << "putToAddress: Connecting to "<< address << "..." << std::endl;

    int ret = connect_to_server(address.c_str(), port, &connectionToServer);
    if(ret!=0)
    {
        logFile <<"ERROR putToAddress: Cannot connect to "<<address<< std::endl;
        std::cout <<"ERROR putToAddress: Cannot connect to "<<address<< std::endl;
        file.close(); 
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
        file.close(); 
        return true;
    }   
}

bool FileSystem::getFromAddress(std::string address, std::string localFile, std::string remoteFile)
{
    int connectionToServer; //TCP connect to introducer/other nodes

    logFile << "getFromAddress: Connecting to "<< address << "..." << std::endl;

    int ret = connect_to_server(address.c_str(), port, &connectionToServer);
    if(ret!=0)
    {
        logFile <<"ERROR getFromAddress: Cannot connect to "<<address<< std::endl;
        std::cout <<"ERROR getFromAddress: Cannot connect to "<<address<< std::endl;
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

        delete buffer;

        close(connectionToServer);
        return ret;
    }
}


void FileSystem::pull(std::string address, std::string filename)
{

}

void FileSystem::push(std::string address, std::string filename)
{

}

FileSystem::FileEntry::FileEntry(std::string name){
    this->filename = name;
    this->key = hashString(this->filename);
}

bool FileSystem::FileEntry::operator<( FileEntry const & second) const{
    return this->key < second.key ? true : false;
}
bool FileSystem::FileEntry::operator>( FileEntry const & second) const{
    return this->key > second.key ? true : false;
}
bool FileSystem::FileEntry::operator==( FileEntry const & second) const{
    return (this->filename.compare(second.filename) == 0) ? true : false;
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

bool FileSystem::VirtualNode::operator!=(VirtualNode const & second) const{
    return this->key != second.key ? true : false;
}

bool FileSystem::pullFileFromRange( std::string address, int minKey, int maxKey, bool rmRemoteFile ){
    int connectionToServer; //TCP connect to introducer/other nodes

    logFile << "pullFileFromRange: Connecting to "<< address << "..." << std::endl;

    int ret = connect_to_server(address.c_str(), port, &connectionToServer);
    if(ret!=0)
    {
        logFile <<"ERROR pullFileFromRange: Cannot connect to "<<address<< std::endl;
        std::cout <<"ERROR pullFileFromRange: Cannot connect to "<<address<< std::endl;
        return false;
    }

    Message_fs msg;
    if(rmRemoteFile)
        msg.type = MSG_PULL_MV;
    else
        msg.type = MSG_PULL_CP;
    msg.minKey = minKey;
    msg.maxKey = maxKey;

    write(connectionToServer, &msg, sizeof(Message_fs)); 
    read (connectionToServer, &msg, sizeof(Message_fs));    //read num of files
    size_t totalCount = msg.size;
    
    for(int i=0; i < totalCount; i++){
        
        read (connectionToServer, &msg, sizeof(Message_fs)); //read filename and size
        std::string filename(msg.filename);

        char * buffer = new char [msg.size];
        read(connectionToServer, buffer,  msg.size);        

        saveFile( filename, buffer, msg.size );

        delete buffer;
    }
    
    return true;
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

    int positionClock = joinPosition - myPosition;
    if(positionClock < 0) 
        positionClock += virtualRing.size() + 1;

    return positionClock;
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

    int positionClock = leavePosition - myOldPosition;
    if(positionClock < 0) 
        positionClock += virtualRing.size() + 1;

    return positionClock;
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

bool FileSystem::myJoinFinished( ){
    
    virtualRingLock.lock();

    int minKey, maxKey;
    bool success;

    VirtualNode target = nNodeLater(3, myPosition);
    if(target != myself ){
        minKey = nNodeBefore(1, myPosition).key;
        maxKey = nNodeBefore(0, myPosition).key;
        success = pullFileFromRange(  target.ip_str, minKey, maxKey, false);
        cout<<success<<" pull key from "<<minKey<<" to "<<maxKey<<" from "<<target.ip_str<<" "<<target.key<<endl;
    }
    

    target = nNodeLater(2, myPosition);
    if(target != myself ){
        minKey = nNodeBefore(2, myPosition).key;
        maxKey = nNodeBefore(1, myPosition).key;
        success = pullFileFromRange(  target.ip_str, minKey, maxKey, false);
        cout<<success<<" pull key from "<<minKey<<" to "<<maxKey<<" from "<<target.ip_str<<" "<<target.key<<endl;
    }

    target = nNodeLater(1, myPosition);
    if(target != myself ){
        minKey = nNodeBefore(3, myPosition).key;
        maxKey = nNodeBefore(2, myPosition).key;
        success = pullFileFromRange(  target.ip_str, minKey, maxKey, false);
        cout<<success<<" pull key from "<<minKey<<" to "<<maxKey<<" from "<<target.ip_str<<" "<<target.key<<endl;
    }

    
    virtualRingLock.unlock();

    return success;
}

bool FileSystem::detectJoin( Node joinNode ){
    addToVirtualRing( joinNode );

    return true;
}

FileSystem::VirtualNode FileSystem::nNodeBefore(int n, int current){
    int position = current - n;
    if(position < 0)
        position += virtualRing.size();
    if(position >= virtualRing.size())
        position -= virtualRing.size();
    
    return virtualRing[position];
}

FileSystem::VirtualNode FileSystem::nNodeLater(int n, int current){
    int position = current + n;
    if(position < 0)
        position += virtualRing.size();
    if(position >= virtualRing.size())
        position -= virtualRing.size();
    
    return virtualRing[position];
}

bool FileSystem::detectLeave( Node leaveNode ){    
    int positionClock = deleteFromVirtualRing( leaveNode );
    
    virtualRingLock.lock();
    int positionAntiClock = virtualRing.size() + 1 - positionClock;

    cout<<"node "<< leaveNode.ip_str<<" leaving "<<positionAntiClock<<" before me."<<endl;
    bool success = false;

    if( positionAntiClock == 3 ){
        VirtualNode target = nNodeBefore(1, myPosition);
        int minKey = nNodeBefore(3, myPosition).key;
        int maxKey = hashString(leaveNode.ip_str);
        success = pullFileFromRange(  target.ip_str, minKey, maxKey, false);
        cout<<success<<" pull key from "<<minKey<<" to "<<maxKey<<" from "<<target.ip_str<<" "<<target.key<<endl;
    }
    else if( positionAntiClock == 2 ){
        VirtualNode target = nNodeBefore(1, myPosition);
        int minKey = nNodeBefore(3, myPosition).key;
        int maxKey = nNodeBefore(2, myPosition).key;
        success = pullFileFromRange(  target.ip_str, minKey, maxKey, false);   
        cout<<success<<" pull key from "<<minKey<<" to "<<maxKey<<" from "<<target.ip_str<<" "<<target.key<<endl;
    }
    else if( positionAntiClock == 1 ){
        VirtualNode target = nNodeBefore(1, myPosition);
        int minKey = nNodeBefore(3, myPosition).key;
        int maxKey = nNodeBefore(2, myPosition).key;
        success = pullFileFromRange(  target.ip_str, minKey, maxKey, false);
        cout<<success<<" pull key from "<<minKey<<" to "<<maxKey<<" from "<<target.ip_str<<" "<<target.key<<endl;
    }

    virtualRingLock.unlock();

    return success;
}

bool FileSystem::myselfLeave( Node myNodeToChange ){
    deleteFromVirtualRing( myNodeToChange );
    
    filesLock.lock();
    files.clear();
    //system("rm files/*");
    filesLock.unlock();

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
            //if(msg.node.ip_str.compare(myNode.ip_str) != 0)
                detectJoin( msg.node );
            //else
                //myselfJoin( msg.node );
        }
        else if(msg.type == MSG_JOIN_FINISH){
            //myJoinFinished( );
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
