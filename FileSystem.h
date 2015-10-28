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

	std::ofstream logFile;	// Log file

	int port; // TCP port to be used for communication

	Membership& membership;

	std::vector< std::string > files;

	std::thread listening;

public: 

	FileSystem(int port, Membership& m);
	~FileSystem();

	void listeningThread();

	void put(std::string address, std::string filename);
	void get(std::string address, std::string filename);

	void pull(std::string address, std::string filename);
	void push(std::string address, std::string filename);


};

#endif