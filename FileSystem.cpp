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

bool FileSystem::putToNode(int nodePosition, std::string localFile, std::string remoteFile)
{
    nodePosition = ( nodePosition + virtualRing.size() ) % virtualRing.size();
    std::string destAddress = virtualRing[nodePosition].ip_str;    
    return putToAddress(destAddress, localFile, remoteFile);
}

bool FileSystem::deleteFromNode(int nodePosition, std::string remoteFile)
{
    nodePosition = ( nodePosition + virtualRing.size() ) % virtualRing.size();
    std::string destAddress = virtualRing[nodePosition].ip_str;    
    return deleteFromAddress(destAddress, remoteFile);
}

bool FileSystem::getFromNode(int nodePosition, std::string localFile, std::string remoteFile){
    nodePosition = ( nodePosition + virtualRing.size() ) % virtualRing.size();
    std::string destAddress = virtualRing[nodePosition].ip_str;    
    return getFromAddress(destAddress, localFile, remoteFile);
}

void FileSystem::put(std::string localFile, std::string remoteFile)
{
    // hashing function to find the machine/s where to store the file;

    ChronoCpu chrono("cpu");
    
    chrono.tic();
    int fileKey = hashString(remoteFile);
    int position = findPositionByKey(fileKey);

    // cout<<"put "<<localFile<<" "<<fileKey<<" to "<<position<<" "<<virtualRing[position].ip_str<<" "<<virtualRing[position].key<<endl;

    bool attempt = putToNode( position, localFile, remoteFile );
    
    attempt = putToNode( position+1, localFile, remoteFile );
    
    attempt = putToNode( position+2, localFile, remoteFile );

    chrono.tac();

    std::cout << "File " << remoteFile << " added in " << chrono.getElapsedStats().lastTime_ms << " ms" << std::endl;
}
void FileSystem::where(std::string remoteFile)
{    
    int fileKey = hashString(remoteFile);
    int position = findPositionByKey(fileKey);

    std::cout<< "Master Replica: " << virtualRing[position].ip_str << std::endl;
    std::cout<< "Second Replica: " << virtualRing[ (position+1) % virtualRing.size()].ip_str << std::endl;
    std::cout<< "Second Replica: " << virtualRing[ (position+2) % virtualRing.size()].ip_str << std::endl;
}

void FileSystem::checkFiles()
{
    // hashing function to find the machine/s where to store the file;
    
    std::vector<std::string> toremove;

    filesLock.lock();

    for ( auto &file : files)
    {
        int flag = false;
        for (int i = 0; i < 3; ++i)
        {
            int position = findPositionByKey( hashString(file.filename) ) + i;
            int nodePosition = ( position + virtualRing.size() ) % virtualRing.size();

            if (virtualRing[nodePosition].key == myself.key)
            {
                flag = true;
            }
        }

        if (!flag)
        {
            toremove.push_back(file.filename);
        }
    }

    for (int i = 0; i < toremove.size(); ++i)
    {
        if ( deleteFile( toremove.at(i) ) ) 
        {
            logFile << toremove.at(i) << "deleted" << std::endl;
        }
        files.remove(toremove.at(i));
    }

    filesLock.unlock();

}

void FileSystem::get(std::string localFile, std::string remoteFile)
{
    //https://gitlab-beta.engr.illinois.edu/remis2/MP3-FileSystem.git
    // hashing function to find the machine where to ask for the file;

    ChronoCpu chrono("cpu");
    
    chrono.tic();

    int fileKey = hashString(remoteFile);
    int position = findPositionByKey(fileKey);

    // cout<<"get "<<localFile<<" "<<fileKey<<" from "<<position<<" "<<virtualRing[position].ip_str<<" "<<virtualRing[position].key<<endl;

    bool attempt = getFromNode( position, localFile, remoteFile );
    if(!attempt)
    {
        std::cout << "Retrying... " << std::endl;
        attempt = getFromNode( position+1, localFile, remoteFile );
    }
    if(!attempt)
    {
        std::cout << "Retrying... " << std::endl;
        attempt = getFromNode( position+2, localFile, remoteFile );
    }

    if (!attempt)
    {
        std::cout << "Error retrieving " << remoteFile << std::endl;
    }

    chrono.tac();
    if(attempt)
        std::cout << "File " << remoteFile << " received in " << chrono.getElapsedStats().lastTime_ms << " ms" << std::endl;
    else
        std::cout << "File " << remoteFile << " cannot be found. returning in " << chrono.getElapsedStats().lastTime_ms << " ms" << std::endl;
}

void FileSystem::deleteFromFS(std::string remoteFile)
{
    // hashing function to find the machine/s where to store the file;
    
    int fileKey = hashString(remoteFile);
    int position = findPositionByKey(fileKey);

    // cout<<"delete " << fileKey << " from "<<position<<" "<<virtualRing[position].ip_str<<" "<<virtualRing[position].key<<endl;

    bool attempt = deleteFromNode( position, remoteFile );
    if( !attempt)
    {
        std::cout << "Problem removing at position " << position << std::endl;
    }
    
    attempt = deleteFromNode( position+1, remoteFile );
    //if(attempt)
        //return;
    
    attempt = deleteFromNode( position+2, remoteFile );
    //if(attempt)
        //return;


    //std::string destAddress = "localhost";
    //put(destAddress, localFile, remoteFile);
}

void FileSystem::listeningThread()
{ 
    int listenFd = open_socket(port); 

    while(true)
    {
        size_t ret;
        int connFd = listen_socket(listenFd);

        logFile<<"listeningThread: someone is contacting me... "<< std::endl;

        Message_fs msg;

        ret = read(connFd, &msg, sizeof(Message_fs));
        std::string filename = msg.filename;
        //cout<<"listening filename: "<<filename<<endl;
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

            if (msg.size == 0)
            {
                std::cout << "File does not exist" << std::endl;
            }
            else
            {
                ret = splitWrite( connFd, buffer, msg.size );
                delete buffer;    
            }
        }
        if(msg.type == MSG_DELETE)
        {
            filesLock.lock();
            for( auto &file : files )
            {
                if( file.filename.compare(filename) == 0)
                {
                    files.remove(file);
                    break;
                }

            }
            filesLock.unlock();
            deleteFile(filename);
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
        return ( (0 <= key && key <= max) || (min < key && key <= RING_SIZE)  );
}

bool FileSystem::sendFileByRange(int connFd, Message_fs msg )
{
    bool deleteMyCopy = ( (msg.type == MSG_PULL_MV) ? true : false );

    logFile<<"sendFileByRange: "<<msg.minKey<<" to "<<msg.maxKey<<endl;

    std::vector<std::string> toremove;
    filesLock.lock();
    size_t count = 0;
    for( auto &file : files ){
        if( inRange(file.key, msg.minKey, msg.maxKey) )
            count++;
    }
    msg.size = count;
    write(connFd, &msg, sizeof(Message_fs));
    for( auto &file : files )
    {
        if( inRange(file.key, msg.minKey, msg.maxKey) )
        {
            memset(msg.filename, '\0', 200);
            file.filename.copy(msg.filename, file.filename.length() );

            // cout<<"sendFileByRange: "<<file.filename<<" "<<msg.filename<<endl;
            logFile<<"sendFileByRange: "<<file.filename<<" rm:"<<deleteMyCopy<<endl;

            char * buffer; 
            msg.size = readFile(file.filename, &buffer);

            write(connFd, &msg, sizeof(Message_fs));
            splitWrite(connFd, buffer, msg.size);

            delete buffer;

            if(deleteMyCopy)
            {
                //files.remove( file );   //maintain file list
                toremove.push_back(file.filename);
            }
        }
    }

    for (int i = 0; i < toremove.size(); ++i)
    {
        if ( deleteFile( toremove.at(i) ) ) 
        {
            logFile << toremove.at(i) << "deleted" << std::endl;
        }
        files.remove(toremove.at(i));
    }

    filesLock.unlock();

    return true;
}

//does not maintain file list!
bool FileSystem::deleteFile( std::string  filename ){
    std::string command = "rm files/" + filename;
    system(command.c_str());
    return true;
}

//Internal for the file system
bool FileSystem::saveFile(std::string filename, char* buffer, size_t length)
{
    bool alreadyThere = false;
    filesLock.lock();  
    for(auto &file : files){
        if(file.filename.compare(filename) == 0){
            alreadyThere = true;
            break;
        }
    }
    filesLock.unlock();

    if(alreadyThere)
        return true;

    std::string root = "files/";
    std::ofstream newfile(root + filename, std::ofstream::binary);

    newfile.write(buffer, length);
    newfile.close();

    FileEntry fileEntry(filename);

    filesLock.lock();
    files.push_back( fileEntry );
    filesLock.unlock();

    logFile << "saveFile " << root + filename << " saved" << std::endl;
    // cout << "saveFile " << root + filename << " saved" << std::endl;

    // cout<<"saveFile: "<<fileEntry.filename<<endl;

    return true;
}

std::string FileSystem::getFileList()
{
    std::stringstream ss;

    checkFiles();

    filesLock.lock();
    for (std::list<FileEntry>::iterator it=files.begin(); it != files.end(); ++it)
    {
        ss << (*it).filename << " " << (*it).key << std::endl;
    }
    filesLock.unlock();

    return ss.str();
}

//Internal for the filesystem
size_t FileSystem::readFile(std::string filename, char** buffer)
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
    size_t length = file.tellg();
    file.seekg (0, file.beg);        

    *buffer = new char [length];

    file.read (*buffer,length);
    file.close();

    logFile << "readFile " << root + filename << " read" << std::endl;
    // std::cout << "readFile " << root + filename << " read" << std::endl;

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

        memset(msg.filename, '\0', 200);
        remoteFile.copy(msg.filename, remoteFile.length());
        msg.filename[remoteFile.length()+1] = '\0';
        msg.size = length;
        //std::string test = msg.filename;
        //std::cout << "test: :" << test << std::endl;

        write(connectionToServer, &msg, sizeof(Message_fs) );
        write(connectionToServer, buffer, length);     

        close(connectionToServer);
        delete buffer;
        file.close(); 
        return true;
    }   
}


bool FileSystem::deleteFromAddress(std::string address, std::string remoteFile)
{
    int connectionToServer; //TCP connect to introducer/other nodes

    // printf("Local file: %s\n", localFile.c_str());
    // printf("Remote file: %s\n", remoteFile.c_str());

    logFile << "Deleting -  Connecting to "<< address << "..." << std::endl;

    int ret = connect_to_server(address.c_str(), port, &connectionToServer);
    if(ret!=0)
    {
        logFile <<"ERROR putToAddress: Cannot connect to "<<address<< std::endl;
        std::cout <<"ERROR putToAddress: Cannot connect to "<<address<< std::endl;
        return false;
    }
    else{

        Message_fs msg;
        msg.type = MSG_DELETE;

        memset(msg.filename, '\0', 200);
        remoteFile.copy(msg.filename, remoteFile.length());
        msg.filename[remoteFile.length()+1] = '\0';
        msg.size = 0;

        write(connectionToServer, &msg, sizeof(Message_fs) );   

        close(connectionToServer);
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
        memset(msg.filename, '\0', 200);
        remoteFile.copy(msg.filename, remoteFile.length());

        write(connectionToServer, &msg, sizeof(Message_fs)); // Send filename
        read (connectionToServer, &msg, sizeof(Message_fs)); // Receive size

        if (msg.size == 0)
        {
            std::cout << "File does not exist: " << remoteFile << std::endl;
            close(connectionToServer);
            return false;
        }
        else
        {
            char * buffer = new char [msg.size];

            //size_t ret = read(connectionToServer, buffer,  msg.size);
            size_t ret =splitRead( connectionToServer, buffer, msg.size );
            // std::cout << "getFromAddress: Received: " << ret <<" of "<<msg.size << std::endl;

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
            return true;
        }        
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

    logFile << "pullFileFromRange: Connecting to "<< address << "... for"<< minKey <<" to "<<maxKey<<" rm:"<<rmRemoteFile << std::endl;
    // cout << "pullFileFromRange: Connecting to "<< address << "..." << std::endl;

    int ret = connect_to_server(address.c_str(), port, &connectionToServer);
    if(ret!=0)
    {
        logFile <<"ERROR pullFileFromRange: Cannot connect to "<<address<< std::endl;
        //std::cout <<"ERROR pullFileFromRange: Cannot connect to "<<address<< std::endl;
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
        //size_t pullReadRet = read(connectionToServer, buffer,  msg.size);
        size_t pullReadRet = splitRead(connectionToServer, buffer,  msg.size);
        // cout<<"pullFileFromRange: pullReadRet "<<pullReadRet<<endl;

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
    // cout<<"detect leave: "<<n.ip_str<<endl;

    VirtualNode changeNode;
    changeNode.ip_str = n.ip_str;
    changeNode.key = hashString(changeNode.ip_str);

    // cout<<"leave virtual node: "<<changeNode.ip_str<<" "<<changeNode.key<<endl;

    virtualRingLock.lock();
    
    int myOldPosition = myPosition;

    int leavePosition = find( virtualRing.begin(), virtualRing.end(), changeNode ) - virtualRing.begin();

    // cout<<"leave node position: "<<leavePosition<<endl;

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

void FileSystem::printVirtualRing()
{
    virtualRingLock.lock();
    for(int i=0; i < virtualRing.size(); i++)
    {
        std::cout << virtualRing[i].ip_str << " " << virtualRing[i].key;
        if (virtualRing[i].key == myself.key)
            std::cout << " X ";
        std::cout << std::endl;
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
        success = pullFileFromRange(  target.ip_str, minKey, maxKey, true);
        // cout<<success<<" pull key from "<<minKey<<" to "<<maxKey<<" from "<<target.ip_str<<" "<<target.key<<endl;
        logFile<<success<<" myJoinFinished pull key from "<<minKey<<" to "<<maxKey<<" from "<<target.ip_str<<" "<<target.key<<endl;
    }
    
    target = nNodeLater(2, myPosition);
    if(target != myself ){
        minKey = nNodeBefore(2, myPosition).key;
        maxKey = nNodeBefore(1, myPosition).key;
        success = pullFileFromRange(  target.ip_str, minKey, maxKey, true);
        // cout<<success<<" pull key from "<<minKey<<" to "<<maxKey<<" from "<<target.ip_str<<" "<<target.key<<endl;
        logFile<<success<<" myJoinFinished pull key from "<<minKey<<" to "<<maxKey<<" from "<<target.ip_str<<" "<<target.key<<endl;
    }

    target = nNodeLater(1, myPosition);
    if(target != myself ){
        minKey = nNodeBefore(3, myPosition).key;
        maxKey = nNodeBefore(2, myPosition).key;
        success = pullFileFromRange(  target.ip_str, minKey, maxKey, true);
        // cout<<success<<" pull key from "<<minKey<<" to "<<maxKey<<" from "<<target.ip_str<<" "<<target.key<<endl;
        logFile<<success<<" myJoinFinished pull key from "<<minKey<<" to "<<maxKey<<" from "<<target.ip_str<<" "<<target.key<<endl;
    }

    virtualRingLock.unlock();

    return success;
}

bool FileSystem::detectJoin( Node joinNode ){
    addToVirtualRing( joinNode );

    return true;
}

FileSystem::VirtualNode FileSystem::nNodeBefore(int n, int current)
{
    int position = current - n;
    while(position < 0)
        position += virtualRing.size();
    while(position >= virtualRing.size())
        position -= virtualRing.size();
    
    return virtualRing[position];
}

FileSystem::VirtualNode FileSystem::nNodeLater(int n, int current)
{
    int position = current + n;
    while(position < 0)
        position += virtualRing.size();
    while(position >= virtualRing.size())
        position -= virtualRing.size();
    
    return virtualRing[position];
}

bool FileSystem::detectLeave( Node leaveNode )
{    
    int positionClock = deleteFromVirtualRing( leaveNode );
    
    virtualRingLock.lock();
    int positionAntiClock = virtualRing.size() + 1 - positionClock;

    // cout<<"node "<< leaveNode.ip_str<<" leaving "<<positionAntiClock<<" before me."<<endl;
    bool success = false;

    int minKey, maxKey;
    if( positionAntiClock == 3 ){
        VirtualNode target = nNodeBefore(1, myPosition);
        minKey = nNodeBefore(3, myPosition).key;
        maxKey = hashString(leaveNode.ip_str);
        success = pullFileFromRange(  target.ip_str, minKey, maxKey, false);
        // cout<<success<<" pull key from "<<minKey<<" to "<<maxKey<<" from "<<target.ip_str<<" "<<target.key<<endl;    
        logFile<<success<<" detectLeave pull key from "<<minKey<<" to "<<maxKey<<" from "<<target.ip_str<<" "<<target.key<<endl;    
    }
    else if( positionAntiClock == 2 ){
        VirtualNode target = nNodeBefore(1, myPosition);
        minKey = nNodeBefore(3, myPosition).key;
        maxKey = nNodeBefore(2, myPosition).key;
        success = pullFileFromRange(  target.ip_str, minKey, maxKey, false);   
        // cout<<success<<" pull key from "<<minKey<<" to "<<maxKey<<" from "<<target.ip_str<<" "<<target.key<<endl;
        logFile<<success<<" detectLeave pull key from "<<minKey<<" to "<<maxKey<<" from "<<target.ip_str<<" "<<target.key<<endl;    
    }
    else if( positionAntiClock == 1 ){
        VirtualNode target = nNodeBefore(1, myPosition);
        minKey = nNodeBefore(3, myPosition).key;
        maxKey = nNodeBefore(2, myPosition).key;
        success = pullFileFromRange(  target.ip_str, minKey, maxKey, false);
        // cout<<success<<" pull key from "<<minKey<<" to "<<maxKey<<" from "<<target.ip_str<<" "<<target.key<<endl;
        logFile<<success<<" detectLeave pull key from "<<minKey<<" to "<<maxKey<<" from "<<target.ip_str<<" "<<target.key<<endl;    
    }

    if(!success){
        VirtualNode target = nNodeBefore(2, myPosition);
        success = pullFileFromRange(  target.ip_str, minKey, maxKey, false);
        // cout<<success<<" pull key second from "<<minKey<<" to "<<maxKey<<" from "<<target.ip_str<<" "<<target.key<<endl;
        logFile<<success<<" detectLeave pull key from "<<minKey<<" to "<<maxKey<<" from "<<target.ip_str<<" "<<target.key<<endl;    
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
            // cout<<"recv join msg: "<<msg.node.ip_str<<endl;
            //if(msg.node.ip_str.compare(myNode.ip_str) != 0)
                detectJoin( msg.node );
            //else
                //myselfJoin( msg.node );
        }
        else if(msg.type == MSG_JOIN_FINISH){
            myJoinFinished( );
        }
        else if(msg.type == MSG_LEAVE || msg.type == MSG_FAIL ){
            // cout<<"recv leave msg: "<<msg.node.ip_str<<endl;
            if( msg.node.ip_str.compare(myNode.ip_str) != 0 )
                detectLeave( msg.node );
            else
                myselfLeave( msg.node );
        }
        //printVirtualRing();
    }
}
