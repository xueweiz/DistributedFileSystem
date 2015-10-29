#ifndef _FileSystem_H_
#define _FileSystem_H_

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>

#include "Membership.h"

class FileSystem
{

private: 

	typedef enum MessageType
	{
	    MSG_PUT,
	    MSG_GET,
	    MSG_DELETE,
	    MSG_EMPTY
	} messageType;

	struct Message_fs {
		messageType type;
		char filename[200];
		char carrier[4];
		size_t size;
	};

	std::ofstream logFile;	// Log file

	int port; // TCP port to be used for communication

	Membership& membership;

	std::vector< std::string > files;

	std::thread listening;

	void listeningThread();

	void put(std::string address, std::string localFile, std::string remoteFile);
	void get(std::string address, std::string localFile, std::string remoteFile);

	void saveFile(std::string filename, char* buffer, size_t length);
	int  readFile(std::string filename, char** buffer);

	void pull(std::string address, std::string filename);
	void push(std::string address, std::string filename);


public: 

	FileSystem(int port, Membership& m);
	~FileSystem();

	void put(std::string localFile, std::string remoteFile);
	void get(std::string localFile, std::string remoteFile);

};

#endif