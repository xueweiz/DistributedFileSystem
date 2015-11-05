#ifndef _Membership_H_
#define _Membership_H_

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>

struct Node 
{
	std::string ip_str;	 // IP address in string format for easy handling;
	int timeStamp;
	int active;
};

typedef enum MessageType
{
    MSG_PING,
    MSG_ACK,
    MSG_PIGGY,
    MSG_PIGGY_PING,
    MSG_PIGGY_ACK,

    MSG_FAIL,
    MSG_JOIN,			//only for TCP connection
    MSG_LEAVE,
    MSG_JOIN_FINISH,

    MSG_ELECTION,		// Node that detect failure of leader
    MSG_BULLY,			// Node with higher is is bullying
    MSG_NEWLEADER,		// Proclamation of a leader
    MSG_NEWLEADER_ACK,	// Proclamation of a leader ACK

    MSG_EMPTY
} messageType;

struct Message {
	messageType type;
	uint8_t roundId;
	char carrierAdd[4];
	int timeStamp;
	char TTL;
};

class MemberUpdateMsg{
public:
	messageType type;
	Node node;
	bool fileSystemRead;
	MemberUpdateMsg(messageType type, Node node);
	MemberUpdateMsg( MemberUpdateMsg const &  msg);
	MemberUpdateMsg & operator= ( MemberUpdateMsg const &  msg);
};

class Membership
{

private: 

	std::ofstream logFile;	// Log file

	std::vector<std::string> address;
	std::vector<Node> nodes;    //store initial nodes

	std::mutex membersLock;
	std::vector<Node> members;  //store members in the group
	Node leader;

	int port, sockfd;   //for UDP connection

	std::mutex roundLock;
	int roundId;    //used by detect thread.

	bool isIntroducer;

	int myTimeStamp;
	std::string my_ip_str;

	std::vector<Message> msgQueue;
	std::mutex msgQueueLock;

	std::thread listening;
	std::thread detecting;
	std::thread joinThread;

	bool killListeningThread;
	bool killDetectingThread;

	bool bullied;

	//List joining methods  -  TCP methods
	void forJoinThread();
	void getAdress(std::string filename);
	bool firstJoin();
	int sendBackLocalList(int connFd);
	int broadcastJoin(Message income, int i);

	//Income messages processing methods - UDP methods
	void listeningThread();
	void joinMsg 	 ( Message msg, std::string sender );
	void leaveMsg	 ( Message msg, std::string sender );
	void failMsg 	 ( Message msg, std::string sender );
	void pingMsg 	 ( Message msg, std::string sender );
	void ackMsg 	 ( Message msg, std::string sender );
	void piggyMsg    ( Message msg, std::string sender );
	void piggyPingMsg( Message msg, std::string sender );
	void piggyAckMsg ( Message msg, std::string sender );
	
	//Failure detection methods
	void detectThread();
	void spreadMessage(Message msg, int i = 0);
	void failureDetected(Node process); // This is the method we call when we detect a failure
	void sendPing(int sockfd, std::string dest, int port, int roundId, std::string carrier);
	void sendAck (int sockfd, std::string dest, int port, int roundId, std::string carrier);

	//Leader election methods, using Bully Algorithm and leader with highest ip add
	void runLeaderElection();
	void checkLeader (); //Method will check the leader status
	void electionMsg ( Message msg, std::string sender ); // Update the leader, replay ok.
	void bullyMsg 	 ( Message msg, std::string sender );
	void newLeaderMsg( Message msg, std::string sender );
	bool isBiggerThanMine(std::string other, std::string mine);

	//Membership modifiers methods
	int addMember  (std::string newMember, int timeStamp); //if already exist, return 1. else return 0
	int checkMember(std::string ip_str); //check IP
	int checkMember(std::string ip_str, int timeStamp); //check IP + timeStamp
	int failMember(std::string ip_str, int timeStamp); //if already failed, return 1. else return 0

	//Queue handlers
	bool msgQueueEmpty();
	Message popMsgQueue();
	void pushMsgQueue(Message msg);
	int queueSize();
	bool ackMsgQueue();

	//message queue to communicate to file system
	std::vector<MemberUpdateMsg> fileSysMsgQueue;
	std::mutex fileSysMsgQueueLock;	
	std::condition_variable fileSysMsgQueueCV;
	void pushMsgToFileSysQueue(MemberUpdateMsg msg);

public: 
	//file system call this method to pull member update message. no busy wait.
	MemberUpdateMsg pullMsgFromFileSysQueue();
	bool emptyFileSysQueue();

	Membership(bool introducer, int port);

	~Membership();

	void join();
	void leave();
	std::string printMember();
	std::string getLeader();

	std::vector<Node> getMembershipList();
	Node getMyNode();
};

#endif