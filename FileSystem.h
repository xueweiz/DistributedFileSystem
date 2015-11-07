#ifndef _FileSystem_H_
#define _FileSystem_H_

#include <iostream>
#include <cstdlib>
#include <fstream>
#include <string>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <list>
#include <algorithm>
#include <functional>
#include <mutex>

#include "Membership.h"

using namespace std;

int hashString(std::string s);

class FileSystem
{

private: 

	typedef enum MessageType
	{
	    MSG_PUT,	//support file save and load
	    MSG_GET,
	    MSG_DELETE,

	    MSG_PULL_CP, //support new node join or leave
	    MSG_PULL_MV, 

	    MSG_EMPTY
	} messageType;

	struct Message_fs {
		messageType type;
		char filename[200];
		char carrier[4];
		size_t size;
		int minKey;
		int maxKey;
	};

	class VirtualNode{
	public:
		std::string ip_str;
		int key;
		bool operator<( VirtualNode const & second) const;
		bool operator>( VirtualNode const & second) const;
		bool operator==( VirtualNode const & second) const;
		bool operator!=( VirtualNode const & second) const;
	};

	class FileEntry{
	public:
		std::string filename;
		int key;
		FileEntry(std::string name);
		bool operator<( FileEntry const & second) const;
		bool operator>( FileEntry const & second) const;
		bool operator==( FileEntry const & second) const;
	};

	std::ofstream logFile;	// Log file

	int port; // TCP port to be used for communication

	Membership& membership;

	std::list< FileEntry > files;
	std::mutex filesLock;

	std::vector< VirtualNode > virtualRing;
	std::mutex virtualRingLock;	//mutex is only used in public interface. not the inside helper function
	int myPosition;
	VirtualNode myself;
	Node myNode;

	std::thread listening;
	std::thread ringUpdating;

	void listeningThread();

	bool putToNode(int nodePosition, std::string localFile, std::string remoteFile);
	bool getFromNode(int nodePosition, std::string localFile, std::string remoteFile);
	bool deleteFromNode(int nodePosition, std::string remoteFile);

	bool putToAddress(std::string address, std::string localFile, std::string remoteFile);
	bool getFromAddress(std::string address, std::string localFile, std::string remoteFile);
	bool deleteFromAddress(std::string address, std::string remoteFile);

	//bool getFromSocket(int connectionToServer, std::string localFile, std::string remoteFile);
	
	bool saveFile(std::string filename, char* buffer, size_t length);
	size_t  readFile(std::string filename, char** buffer);
	bool deleteFile( std::string  filename );

	void pull(std::string address, std::string filename);
	void push(std::string address, std::string filename);

	//given a socket, pull files from other node
	//this function will be called when a new join or leave happens
	bool pullFileFromRange( std::string address, int minKey, int maxKey, bool rmRemoteFile );
	bool sendFileByRange(int connFd, Message_fs msg);

	/****** Virtual Ring Construction & Maintain *******/
	//add new joining node&hash to virtual ring, update myPosition. return the relative position to new node 
	int addToVirtualRing( Node n );
	//delete the leaving node&hash to virtual ring, update myPosition. return the relative position to new node 
	int deleteFromVirtualRing( Node n );

	VirtualNode nNodeBefore(int n, int current);
	VirtualNode nNodeLater(int n, int current);

	/****** Request Virtual Ring Info *******/
	//get the virtual node position after you by n position
	int getNextInVirtualRing( int n );
	//give a node
	int getVirtualNodeByKey( int key );
	//give a key, find the response node position
	int findPositionByKey( int key );

	void updateThread();

	void checkFiles();


public: 

	FileSystem(int port, Membership& m);
	//FileSystem(int port);

	~FileSystem();

	//when other node is leaving, call detectLeave() to copy files from other machine
	bool detectLeave( Node leaveNode );

	//when other node is joining. Only add to virtual ring, do nothing with file
	bool detectJoin( Node joinNode );
	//bool myselfJoin( Node myNodeToChange );

	//when the join() method in membership finish, call this, move files from other machine 
	bool myJoinFinished( );

	//when myself is leaving, update virtual ring
	bool myselfLeave( Node myNodeToChange );

	std::string getFileList();

	void put(std::string localFile, std::string remoteFile);
	void get(std::string localFile, std::string remoteFile);

	void printVirtualRing();

	void deleteFromFS(std::string remoteFile);
	void where(std::string remoteFile);

};

#endif