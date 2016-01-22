Small Distributed File System
======================
SDFS is a hdfs-like file system. A few important characters: 

1. Immutable files. 

2. Transparent to user. As a user, you don't need to care about the details about server layout. Simple APIs are provided to do file upload and download: 

	void FileSystem::put(std::string localFile, std::string remoteFile);

	void FileSystem::get(std::string localFile, std::string remoteFile);

3. Fault tolerant. Each file in distributed file system has three replicas. So we can handle two simultanous failure on our server. After machine's failure, all replicas on it will be replicated on other machines. 

4. De-centralized. We use a Ring-based key-value store approach to store the data. 

5. Efficient. The failure detection module is based on SWIM protocol, which has O(1) workload when there is no failure. 

For design details, please check out the report (pdf file). 

Code Structure
--------------
### Address.add, AddrIntro.add
Store the IP address or hostname of you machines. One should be put in AddrIntro.add, all the others in Address.add.

### Chrono
Timing library. 

### FileSystem
Support for distributed file system manipulation. 

### Membership
Membership control module using SWIM-protocol. It detects machine joining/leaving/failure. 

### Connections
Helper methods for TCP and UDP connections, and robust data transfer. 

Authors
--------------
Luis Remis (remis2 at illinois dot edu)

Xuewei Zhang (xzhan160 at illinois dot edu)
