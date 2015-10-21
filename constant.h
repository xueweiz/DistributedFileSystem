#ifndef CONSTANT_H
#define CONSTANT_H

typedef enum MessageType
{
    MSG_PING,
    MSG_ACK,
    MSG_PIGGY,
    MSG_PIGGY_PING,
    MSG_PIGGY_ACK,
    MSG_FAIL,
    MSG_JOIN,		//only for TCP connection
    MSG_LEAVE,
    MSG_EMPTY
} messageType;

struct Message {
	messageType type;
	uint8_t roundId;
	char carrierAdd[4];
	int timeStamp;
	char TTL;
};

/*
	For Ping and Ack message. 
	if TTL == 0
		carrierAdd is the pinged or acked target
	if TTL == 1
		carrierAdd is the 
	alway reply to sender, not carrierAdd
*/

struct Node {
	string ip_str;	 // IP address in string format for easy compare;
	int timeStamp;
	int active;		//if active==1, it is alive. If out of reach, set to -3. Each round, increase 1. When reach 0, delete.
};


#define BUFFER_MAX (521)
#define NODES_NUMBER (7)
#define K_FORWARD (4)

//  in ms
#define SLEEP_TIME (50)
#define MAX_LATENCY (200*1000)	//0.1s

#endif