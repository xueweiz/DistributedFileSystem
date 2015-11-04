#ifndef _FileSystem_H_
#define _FileSystem_H_

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <functional>
#include <mutex>

#include "Membership.h"

using namespace std;

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
	};

	class VirtualNode{
	public:
		std::string ip_str;
		int key;
		bool operator<( VirtualNode const & second) const;
		bool operator>( VirtualNode const & second) const;
		bool operator==( VirtualNode const & second) const;
	};

	std::ofstream logFile;	// Log file

	int port; // TCP port to be used for communication

	Membership& membership;

	std::vector< std::string > files;

	std::vector< VirtualNode > virtualRing;
	std::mutex virtualRingLock;	//mutex is only used in public interface. not the inside helper function
	int myPosition;
	VirtualNode myself;
	Node myNode;

	std::thread listening;
	std::thread ringUpdating;

	void listeningThread();

	void put(std::string address, std::string localFile, std::string remoteFile);
	void get(std::string address, std::string localFile, std::string remoteFile);
	
	void saveFile(std::string filename, char* buffer, size_t length);
	int  readFile(std::string filename, char** buffer);

	void pull(std::string address, std::string filename);
	void push(std::string address, std::string filename);

	//given a socket, pull files from other node
	//this function will be called when a new join or leave happens
	bool pullFileFromRange( int socketFd, int min, int max, bool rmRemoteFile );

	/****** Virtual Ring Construction & Maintain *******/
	//add new joining node&hash to virtual ring, update myPosition. return the relative position to new node 
	int addToVirtualRing( Node n );
	//delete the leaving node&hash to virtual ring, update myPosition. return the relative position to new node 
	int deleteFromVirtualRing( Node n );
	void printVirtualRing();

	/****** Request Virtual Ring Info *******/
	//get the virtual node position after you by n position
	int getNextInVirtualRing( int n );
	//give a node
	int getVirtualNodeByKey( int key );

	void updateThread();

	int hashString(std::string s);


public: 

	FileSystem(int port, Membership& m);
	//FileSystem(int port);

	~FileSystem();

	//when other node is leaving, call detectLeave() to copy files from other machine
	bool detectLeave( Node leaveNode );

	//when other node is joining. Only add to virtual ring, do nothing with file
	bool detectJoin( Node joinNode );

	//when myself is joining, call myselfJoin() to move files from other machine
	bool myselfJoin( Node myNodeToChange );

	//when myself is leaving, update virtual ring
	bool myselfLeave( Node myNodeToChange );

	void put(std::string localFile, std::string remoteFile);
	void get(std::string localFile, std::string remoteFile);

};

#endif