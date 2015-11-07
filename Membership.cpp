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
#include "Membership.h"

#include "FileSystem.h"

Membership::Membership(bool introducer, int port)
{
	logFile.open("logMembership.log");

	isIntroducer = introducer;
	this->port = port;
	leader.active = false;

	sockfd = bindSocket(port);

    if(isIntroducer){
        logFile << "I am the introducer! " << std::endl;
        getAdress("Address.add");
    }
    else{
        getAdress("AddrIntro.add");
    }

    roundLock.lock();
    roundId = 0;
    roundLock.unlock();

    join();

    std::thread th3(&Membership::forJoinThread, this);
    std::thread th1(&Membership::listeningThread, this);
    std::thread th2(&Membership::detectThread, this);

    listening.swap(th1);
    detecting.swap(th2);
    joinThread.swap(th3);

    killListeningThread = false;
	killDetectingThread = false;

	//runLeaderElection();
}

Membership::~Membership()
{
	//Kill threads
	killListeningThread = true;
	killDetectingThread = true;

	listening.join();
	detecting.join();
}

void Membership::join()
{
	bool joined = firstJoin();
    while( !isIntroducer && !joined)
    {   //introducer will firstJoin() once. Other node will keep firstJoin() until it enter the group.
        joined = firstJoin();
        //usleep( 1000*1000 );
    }


    MemberUpdateMsg msg( MSG_JOIN_FINISH , members[0]);
    pushMsgToFileSysQueue(msg);
}

void Membership::leave()
{
    membersLock.lock();
    Message msg;
    msg.type = MSG_LEAVE;
    msg.TTL = 1; // Just in case
    ipString2Char4(members.at(0).ip_str, msg.carrierAdd);
    msg.timeStamp = members.at(0).timeStamp;
    membersLock.unlock();
    spreadMessage(msg); // this method wants the lock!
    
    while(members.size()!=0){
        failMember(members[0].ip_str, members[0].timeStamp);
    }
} 

/* Get address from other nodes: */
void Membership::getAdress(std::string filename)
{
	logFile<<"getAdress:" << std::endl;
    std::ifstream addFile(filename);

    std::string str;
    int i=0; 

    address.clear();
    
    while(!addFile.eof())
    {
        getline(addFile, str);
        address.push_back(str);
        struct hostent *server = gethostbyname(str.c_str());

        struct in_addr addr;
        char ip[20];
        memcpy(&addr, server->h_addr_list[0], sizeof(struct in_addr)); 
        strcpy(ip,inet_ntoa(addr));
        
        std::string ip_str (ip);

        struct Node newnode;
        //newnode.name = str;
        newnode.ip_str = ip_str;

        nodes.push_back(newnode);
        
        logFile << "read node " << i++ << " from addr file: " << str << " : " << ip_str << std::endl;
    }
}

bool Membership::firstJoin()
{
    logFile<<"calling firstJoin"<< std::endl;

    //set my own addr, ip, timeStamp, roundID
    myTimeStamp = time(NULL);
    my_ip_str = getOwnIPAddr();
    
    //now I have my self as member
    addMember( my_ip_str, myTimeStamp );

    bool joined = false;

    Message msg;
    msg.type = MSG_JOIN;
    msg.roundId = -1;

    ipString2Char4(my_ip_str, msg.carrierAdd);

    msg.timeStamp = myTimeStamp;
    msg.TTL = isIntroducer;	//used to distinguish joining node is Introducer or not

    for(int i=0; (i < nodes.size()) /*&& !joined*/ ; i++){
        int connectionToServer;
        //TCP connect to introducer/other nodes
        logFile <<"Join: Connecting to "<< nodes[i].ip_str << "..." << std::endl;

        int ret = connect_to_server(nodes[i].ip_str.c_str(), port + 1, &connectionToServer);
        if(ret!=0)
        {

            logFile <<"ERROR Join: Cannot connect to "<<nodes[i].ip_str<< std::endl;
            if(!isIntroducer){
            	i--;
            	usleep(200*1000);
            }
            continue;
        }
        else{
            ret = write(connectionToServer, &msg, sizeof(Message) );
            
            Message newMsg;
            read(connectionToServer, &newMsg, sizeof(Message));

            int size = newMsg.timeStamp;
            
            Message * msgs = new Message[size];

            int j=0;
            for(j=0; j < size; j++){
                read(connectionToServer, msgs + j, sizeof(Message));
                logFile<<"FirstJoin: received "<<char42String(msgs[j].carrierAdd)<<" "<<msgs[j].timeStamp<< std::endl;
            }

            if(j == size){
                joined = true;
                for(j=0; j < size; j++)
                	addMember(char42String(msgs[j].carrierAdd), msgs[j].timeStamp);
            }
            else{
                std::cout << "Join: Failed downloading nodes list"<< std::endl;
            	logFile   <<"ERROR Join: Failed downloading nodes list"<< std::endl;
            }

            delete [] msgs;
            close(connectionToServer);
        }    
    }

    //printMember();

    return joined;
}

void Membership::forJoinThread(){ // Will run only in the introducer
    int listenFd = open_socket(port + 1);   //use the port next to UDP as TCP port
    while(true)
    {
        int ret;
        int connFd = listen_socket(listenFd);

        logFile<<"ForJoinThread: one node asking for membership list"<< std::endl;

        Message income;
        //char addr[4];

        read(connFd, &income, sizeof(income));
        
        //if this is normal node: 
        	//if carrier is introducer, send back local list, then add carrier node
        	//else, just add carrier node
        //if this is introducer node:
        	//send back local list. add carrier node. tell everyone carrier node is joining.

        //me is a normal node
        if(!isIntroducer){
        	//if introducer is joining
        	if(income.TTL == 1){
        		//give introducer my local list
        		sendBackLocalList(connFd);
        	}
        	//no matter what is the joining node, join it
        	addMember(char42String(income.carrierAdd), income.timeStamp);
        }
        else{	//this node is an introducer

        	//send back local list
        	sendBackLocalList(connFd);
        	
        	//add carrier node
        	addMember(char42String(income.carrierAdd), income.timeStamp);
        	
        	usleep( 10*1000 );	//wait to make sure last joined member has enough time to open TCP listening port

        	//tell everyone carrier node is joining
        	membersLock.lock();
        	std::thread ** broadThreads = new std::thread*[members.size() ];	//members[0] is introducer itself, members[size-1] is the node just joined
        	for(int i=1; i < members.size() - 1; i++)
        		broadThreads[i] = new std::thread(&Membership::broadcastJoin, this, income, i);
        	for(int i=1; i < members.size() - 1; i++){
        		broadThreads[i]->join();
        		delete broadThreads[i];
        	}
        	delete [] broadThreads;
        	membersLock.unlock();
        }

        close(connFd);
        
        //printMember();
    }
    return;
}

int Membership::broadcastJoin(Message income, int i)
{
	income.TTL = 0;
	int connectionToServer;
    //TCP connect to members of introducer
    logFile<<"broadcastJoin: Introducer trying to broadcast join to "<<members[i].ip_str << std::endl;
    int ret = connect_to_server(members[i].ip_str.c_str(), port + 1, &connectionToServer);
    if(ret!=0){
        logFile<<"broadcastJoin: cannot connect to "<<members[i].ip_str << std::endl;
        return 1;
    }
    else{
    	ret = write(connectionToServer, &income, sizeof(Message) );
    	close(connectionToServer);
    }
    return 0;
}

int Membership::sendBackLocalList(int connFd){
	membersLock.lock();

    int size = members.size();
    
    //prepare all the members
    Message * buffer = new Message[size];
    char addr[4];

    for(int i=0; i < size; i++){
    	buffer[i].type = MSG_JOIN;
		buffer[i].roundId = -1;
		ipString2Char4(members[i].ip_str, addr);
		for(int j=0; j < 4; j++)
			buffer[i].carrierAdd[j] = addr[j];
		buffer[i].timeStamp = members[i].timeStamp;
		buffer[i].TTL = 0;
    }
    
    membersLock.unlock();

    Message sizeMsg;
    sizeMsg.timeStamp = size;
    int ret = write(connFd, &sizeMsg, sizeof(Message) );

    for(int i=0; i < size; i++){
    	ret = write(connFd, buffer+i, sizeof(Message) );
    	logFile<<"sendBackLocalList: sending "<<char42String(buffer[i].carrierAdd)<<" "<<buffer[i].timeStamp<< std::endl;
    }
    delete [] buffer;
    return 0;
}

void Membership::listeningThread()
{
    //char buffer[BUFFER_MAX];
    struct Message msg;
    std::string sender;
    srand (time(NULL));
    logFile<< std::endl <<"listeningThread: thread begins"<<std::endl; 
    while (!killListeningThread)
    {
        if (members.empty())
        {
            //In case the node decided to leave
            continue;
        }

        int byte_count = receiveUDP(sockfd, (char*)&msg, sizeof(msg), sender);

        logFile << "listeningThread: Receive message from: " << sender.c_str() << " ";
        logFile << "type: " << msg.type << "ttl: " << (int)msg.TTL << std::endl;

        if (byte_count != sizeof(msg))
        {
            logFile << "ERROR listeningThread: size receiving incorrect: Message dropped" << std::endl;
            continue;
        }
        
        switch (msg.type)
        {
            case MSG_PING:  
                pingMsg(msg, sender);
                break;
            case MSG_ACK:
                ackMsg(msg, sender);
                break;
            case MSG_PIGGY:         
                piggyMsg(msg, sender);
                break;
            case MSG_PIGGY_PING:    
                piggyPingMsg(msg, sender);
                break;
            case MSG_PIGGY_ACK:     
                piggyAckMsg(msg, sender);
                break;
            case MSG_FAIL:          
                failMsg(msg, sender);
                break;
            case MSG_JOIN:          
                joinMsg(msg, sender);
                break;
            case MSG_LEAVE:         
                leaveMsg(msg, sender);
                break;
            case MSG_ELECTION:         
                electionMsg(msg, sender);
                break;
            case MSG_BULLY:         
                bullyMsg(msg, sender);
                break;
            case MSG_NEWLEADER:         
                newLeaderMsg(msg, sender);
                break;
            
            default:
        	   logFile<<"ERROR: listeningThread: received msg does not belong to a type"<<std::endl;
        }

    }
}

void Membership::detectThread()
{
    /*
    WHILE LOOP
        roundId++

        randomly select one node
        send ping message
        sleep(1)
        see msgQueue, search ack
        if(ack)
            cout<<alive
            sleep(4)
            continue
        
        send ping message to random K nodes (call spreadMessage())
        sleep(4)
        see msgQueue, search ack
        if(ack)
            cout<<alive
            continue
        
        if(!ack)
            cout<<fail
            delete node
            send fail message to other nodes
            continue
    */
    bool flagFail = false;
    Node failedNode;
    while(!killDetectingThread)
    {
  
        roundId = (roundId+1)%255;

        if (roundId % 5 == 0)
        {
        	checkLeader();
        }

        logFile<< std::endl << "Detection Thread - Round: "<< roundId << " ------------" << std::endl;
        logFile<<printMember();
        
        if(members.size() < 2)
        {
            usleep(5 * MAX_LATENCY);
            flagFail = false;
            continue;
        }

        int select = rand()%(members.size()-1) + 1;
        Node theNode = members[select];

        if (flagFail == true)
        {
            theNode = failedNode;
        }

        Message msg;
        msg.type = MSG_PING;
        msg.TTL = 0;
        msg.roundId = roundId;
        ipString2Char4(theNode.ip_str, msg.carrierAdd);
        msg.timeStamp = 0;

        logFile<<"detectThread: checking alive or not for "<<theNode.ip_str<<" "<<theNode.timeStamp<<std::endl;
        sendUDP(sockfd, theNode.ip_str, port, (char*)&msg, sizeof(Message));

        bool acked = false;

        if (queueSize() > 10){
            std::cout << "Warning!!! queueSize = " << queueSize() << std::endl;
        }

        usleep(MAX_LATENCY);

        msgQueueLock.lock();
        acked = ackMsgQueue();
        msgQueueLock.unlock();

        static int count = 0;
        if(acked){
            logFile<<"detectThread: node alive: "<<theNode.ip_str<<" "<<theNode.timeStamp<<std::endl;
            flagFail = false;
            usleep(4 * MAX_LATENCY);     
            continue;       
        }

        logFile<<"Hey!!! Node "<<theNode.ip_str<<" did not respond the first time!" << std::endl;

        msg.type = MSG_PIGGY;
        msg.TTL = 0;
        spreadMessage(msg,3);
        usleep(4 * MAX_LATENCY);

        msgQueueLock.lock();
        acked = ackMsgQueue();
        msgQueueLock.unlock();

        if(acked){
            logFile<<"detectThread: second round found node alive: "<<theNode.ip_str<<" "<<theNode.timeStamp<<std::endl;
            flagFail = false;
            continue;
        }
        else{
            logFile<< "detectThread: No ack received: node failed: "<<theNode.ip_str<<" "<<theNode.timeStamp<<std::endl;
            
            if (flagFail == true)
            {
                logFile<< "detectThread: No ack received: node failed: "<<theNode.ip_str<<" "<<theNode.timeStamp<<std::endl;
                failMember(theNode.ip_str, theNode.timeStamp);

                Message failMsg;
                failMsg.type = MSG_FAIL;
                failMsg.roundId = roundId;
                ipString2Char4(theNode.ip_str, failMsg.carrierAdd);
                failMsg.timeStamp = theNode.timeStamp;
                failMsg.TTL = 3;

                spreadMessage(failMsg);
                flagFail = false;
            }
            else
            {
                logFile<< "detectThread: Robust 2: It is alive: "<<theNode.ip_str<<" "<<theNode.timeStamp<<std::endl;
                failedNode = theNode;
                flagFail = true;

            }


            continue;
        }
    }
}

/*
	call rejoin()	//only when me is false-failured
*/
void Membership::joinMsg( Message msg, std::string sender ){
	//if you get this message, it means you has been failed by other nodes.
	//this means false-failure
	//you should call rejoin()
}

/*
	fail the carrier node
	logFile<<carrier failed
	msg.TTL--;
	spreadMessage(msg) 
*/
void Membership::leaveMsg( Message msg, std::string sender )
{
    std::string carrier = char42String(msg.carrierAdd);

    if ( !checkMember(carrier) ) return; // Already deleted

	failMember(carrier, msg.timeStamp);
    
    logFile<<"leaveMsg: node "<<carrier<<" has left"<<std::endl;

    if(msg.TTL == 0)
    {
        return;
    }
    else{
        msg.TTL--;
        spreadMessage(msg);
    }
}

/*
	fail the carrier node
	logFile<<carrier failed
	msg.TTL--;
	spreadMessage(msg) 
*/
void Membership::failMsg( Message msg, std::string sender )
{
	std::string carrier = char42String(msg.carrierAdd);
/*
    if ( my_ip_str.compare(carrier) == 0) // THIS IS MY! I NEED TO REJOIN
    {
        std::cout <<"they are trying to kill me!" << std::endl;
        membersLock.lock();
        members.clear();
        membersLock.unlock();
        usleep( 2000*1000 ); // wait 2 second and rejoin
        bool joined = firstJoin();
        while( !isIntroducer && !joined)
        {   //introducer will firstJoin() once. Other node will keep firstJoin() until it enter the group.
            joined = firstJoin();
            usleep( 1000*1000 );
        }
        return;
    }
*/
    failMember(carrier, msg.timeStamp);
	logFile<<"failMsg: node "<<carrier<<" failed acording to " << sender << std::endl;
/*
    int select = rand()%10;

    if (select <= 7) // 1 and 2
    //if (true) // 1 and 2
    {
        Message justInCase;
        justInCase.type = MSG_FAIL;
        msg.TTL = 0;
        ipString2Char4(carrier, justInCase.carrierAdd);
        sendUDP( sockfd,  carrier, port, (char*)&justInCase, sizeof(Message) );
        // Just in case let the node know so it can rejoin
    }
*/
    if(msg.TTL==0)
    {
        return;
    }
    else{
    	msg.TTL--;
    	spreadMessage(msg);
    }
}

void Membership::pingMsg( Message msg, std::string sender )
{
	msg.type = MSG_ACK;
	sendUDP( sockfd,  sender, port, (char*)&msg, sizeof(Message) );
}

void Membership::ackMsg( Message msg, std::string sender )
{
    msgQueueLock.lock();
    pushMsgQueue(msg);
    msgQueueLock.unlock();
}

void Membership::piggyMsg( Message msg, std::string sender )
{
    std::string target = char42String(msg.carrierAdd);
    msg.type = MSG_PIGGY_PING;

    ipString2Char4(sender, msg.carrierAdd); //Original Sender as carrier

    sendUDP( sockfd,  target, port, (char*)&msg, sizeof(Message) );
    logFile << "Weired!: " << sender << " says that " << target << " did not responde..." << std::endl;
}

void Membership::piggyPingMsg( Message msg, std::string sender )
{
    msg.type = MSG_PIGGY_ACK;
    //ipString2Char4(char42String(msg.carrierAdd), msg.carrierAdd); //Original Sender as carrier
    sendUDP( sockfd,  sender, port, (char*)&msg, sizeof(Message) );

    //Just in case, we can send the message back to the original sender
    // Because we can!
    std::string original = char42String(msg.carrierAdd);
    msg.type = MSG_ACK;
    sendUDP( sockfd,  original, port, (char*)&msg, sizeof(Message) );
}

void Membership::piggyAckMsg( Message msg, std::string sender )
{
    std::string target = char42String(msg.carrierAdd);
    msg.type = MSG_ACK;

    ipString2Char4(sender, msg.carrierAdd); //Dont care, but meh

    sendUDP( sockfd,  target, port, (char*)&msg, sizeof(Message) );
}

bool Membership::isBiggerThanMine(std::string other, std::string mine)
{
	char other_char[4];
	char me_char[4];
	ipString2Char4(other, other_char);
	ipString2Char4(mine, me_char);

	for (int i = 0; i < 4; ++i)
	{
		unsigned int me = (uint8_t) me_char[i];
		unsigned int ot = (uint8_t) other_char[i];
		logFile << ot << " > " << me << std::endl;
		if( ot > me){
			logFile << ot << " > " << me << std::endl;
			return true;
		}
		if( me > ot){
			logFile << ot << " < " << me << std::endl;
			return false;
		}
	}
	
	logFile << "comparing with myself: this code sucks!" << std::endl;
	return false;
}

std::string Membership::getLeader()
{
	if ( leader.active)
	{
		return leader.ip_str;
	}
	return "No leader here, I am my own boss \n";
}

std::vector<Node> Membership::getMembershipList()
{

    membersLock.lock();
    std::vector<Node> ret = members;
    membersLock.unlock();

    return ret;
}


void Membership::runLeaderElection()
{
	leader.active = false;
	bullied = false;

	if (members.size() < 2)
	{
		logFile << "No members, canceling leader election" << std::endl;
		return;
	}

	membersLock.lock();

	for (int i = 1; i < members.size(); ++i)
	{
		std::string other = members.at(i).ip_str;
		if ( isBiggerThanMine( other, my_ip_str) )
		{
			Message msg;
			msg.type = MSG_ELECTION;
			sendUDP( sockfd,  other, port, (char*)&msg, sizeof(Message) );
		}
	}

	membersLock.unlock();

	usleep(500); //Sleep one second

	if ( !bullied ) // Send all other that I am the leader
	{
		membersLock.lock();

		for (int i = 1; i < members.size(); ++i)
		{
			std::string other = members.at(i).ip_str;
			Message msg;
			msg.type = MSG_NEWLEADER;
			sendUDP( sockfd,  other, port, (char*)&msg, sizeof(Message) );
		}

		membersLock.unlock();

		leader = members.at(0);
		leader.active = true;
	}

/*
	Sleep
	check the queue

	if queue not empty, return
	else send NEWLEADER MSG  (I AM THE LEADER)
*/

}

void Membership::checkLeader()
{
	logFile << "Checking leader... " << std::endl;

	if ( !leader.active)
	{
		logFile << "No leader! " << std::endl;
		runLeaderElection();
		return;
	}

	bool exist = false;

	membersLock.lock();

	for (int i = 0; i < members.size(); ++i)
	{
		if( leader.ip_str.compare(members.at(i).ip_str) == 0)
		{
			exist = true;
		}
	}

	membersLock.unlock();

	if (!exist)
	{
		leader.active = false;
		runLeaderElection();
	}
}


void Membership::electionMsg( Message msg, std::string sender )
{
	logFile << "Election received! I am gonna bully that guy :)" << std::endl;
    msg.type = MSG_BULLY;
    ipString2Char4(sender, msg.carrierAdd); //Dont care, but meh

    sendUDP( sockfd,  sender, port, (char*)&msg, sizeof(Message) );

	runLeaderElection();
}

void Membership::bullyMsg 	 ( Message msg, std::string sender )
{
	logFile << "Bully received, someone is bulliyinhg me!!!" << std::endl;
	bullied = true;
}

void Membership::newLeaderMsg( Message msg, std::string sender )
{
	logFile << "New leader recieved... lets check that..." << std::endl;
	bool flag = false;
	if ( isBiggerThanMine(sender, my_ip_str) )
	{
		membersLock.lock();

		for (int i = 0; i < members.size(); ++i)
		{
			if ( 0 == sender.compare(members.at(i).ip_str) )
			{
				logFile<< "Ok, new leader acepted: " <<  sender << std::endl;
				leader = members.at(i);
				leader.active = true;
				flag = true;
			}
			// Message msg;
			// msg.type = MSG_NEWLEADER_ACK;
			// sendUDP( sockfd,  other, port, (char*)&msg, sizeof(Message) );
		}

		membersLock.unlock();

		if(!flag)
		{
			runLeaderElection();
		}
	}
	else
	{
		logFile << "I am bigger than that guy! Re runnig election" <<  sender << std::endl;
		runLeaderElection();
	}

}


std::string Membership::printMember(){

    std::stringstream ret;
    membersLock.lock();
    for(int i=0; i < members.size(); i++){
        ret<<"Member["<<i<<"]: "<<members[i].ip_str<<" "<<members[i].timeStamp<<" "<<members[i].active<<std::endl;
    }
    membersLock.unlock();  

    return ret.str();  
}

//if already exist, return 1. else return 0
int Membership::addMember(std::string newAddress, int timeStamp){
    Node newMember;
    newMember.ip_str = newAddress;
    newMember.timeStamp = timeStamp;
    newMember.active = 1;
    
    membersLock.lock();

    bool exist = false;
    int position = 0;
    for(int i=0; i < members.size(); i++){
        if( members[i].ip_str.compare( newMember.ip_str )==0 ){
            exist = true;
            position = i;
        }
    }
    
    if(exist){
        members[position].timeStamp = newMember.timeStamp;
    }
    else{
        //if add a new member, tell the memUpMsgQueue
        MemberUpdateMsg msg(MSG_JOIN, newMember);
        pushMsgToFileSysQueue(msg);

        members.push_back(newMember);
    }

    membersLock.unlock();

    logFile << "New member: " << newMember.ip_str << std::endl;

    return exist;
}

//check IP
int Membership::checkMember(std::string ip_str){
    bool exist = false;
    
    membersLock.lock();
    
    for(int i=0; i<members.size(); i++){
        if( members[i].ip_str.compare( ip_str )==0 ){
            exist = true;
        }
    }

    membersLock.unlock();

    return exist;
}

//check IP + timeStamp
int Membership::checkMember(std::string ip_str, int timeStamp){
    bool exist = false;
    
    membersLock.lock();
    
    for(int i=0; i<members.size(); i++){
        if( members[i].ip_str.compare( ip_str )==0 && members[i].timeStamp == timeStamp){
            exist = true;
        }
    }

    membersLock.unlock();

    return exist;   
}

//if already failed, return 1. else return 0
int Membership::failMember(std::string ip_str, int timeStamp){

    membersLock.lock();

    bool exist = false;
    int position = 0;

    for(int i=0; i<members.size(); i++){
        if( members[i].ip_str.compare( ip_str )==0 && members[i].timeStamp == timeStamp ){
            exist = true;
            position = i;
            break;
        }
    }
    
    if(exist){
        MemberUpdateMsg msg(MSG_LEAVE, members[position]);
        pushMsgToFileSysQueue(msg);

        members.erase( members.begin()+position );
    }
    
    membersLock.unlock();

    checkLeader();

    return !exist;
}

bool Membership::msgQueueEmpty(){
	bool empty;
	if(msgQueue.size()==0)
		empty = true;
	else
		empty = false;
	return empty;
}

Message Membership::popMsgQueue(){
	Message msg;
	msg.type= MSG_EMPTY;

	if(msgQueue.size()!=0){
		msg = msgQueue.back();
		msgQueue.pop_back();
	}
	
	return msg;
}

void Membership::pushMsgQueue(Message msg){
	msgQueue.push_back(msg);	
	return;
}

int Membership::queueSize(){
	int size = msgQueue.size();
	return size;
}

bool Membership::ackMsgQueue(){
    bool acked = false;
    while(!msgQueueEmpty()){
        Message receive = popMsgQueue();
        if(receive.roundId==roundId)
            acked = true;
    }
    return acked;
}

void Membership::spreadMessage(Message msg, int forwardNumber)
{
    //choose k or size-1 members
    membersLock.lock();
    std::vector<Node> selectedNode = members; 
    membersLock.unlock();

    if (selectedNode.size() < 3) return;   // Not enough nodes to do something
    // We need to remeve myself and the carrier from the possible list

    selectedNode.erase(selectedNode.begin()); // remove myself

    for (int i = 0; i < selectedNode.size(); ++i)
    {
        logFile << "comparing: " << selectedNode.at(i).ip_str << " " << char42String(msg.carrierAdd) << std::endl;
        if ( selectedNode.at(i).ip_str.compare(char42String(msg.carrierAdd)) == 0 )
        {
            selectedNode.erase(selectedNode.begin() + i); // remove carrier
            logFile << "equal" << std::endl;
            break;
        }
    }

    int k = forwardNumber == 0 ? K_FORWARD : forwardNumber;

    while (selectedNode.size() > 0 && k > 0)
    {
        int random = rand() % selectedNode.size();
        sendUDP( sockfd, selectedNode[random].ip_str, port, (char*)&msg, sizeof(Message) );
        selectedNode.erase(selectedNode.begin()+random);
        k--;
    }
}


void Membership::failureDetected(Node process) // This is the method we call when we detect a failure
{
    Message msg;
    msg.type = MSG_FAIL;
    msg.roundId = 0;
    ipString2Char4( process.ip_str, msg.carrierAdd );
    msg.timeStamp = process.timeStamp;
    msg.TTL = K_FORWARD;

    spreadMessage(msg);
}

MemberUpdateMsg::MemberUpdateMsg(messageType type, Node node){
    this->type = type;
    this->node = node;
    this->fileSystemRead = false;
}

MemberUpdateMsg::MemberUpdateMsg( MemberUpdateMsg const &  msg){
    this->type = msg.type;
    this->node = msg.node;
    this->fileSystemRead = msg.fileSystemRead;
}

MemberUpdateMsg & MemberUpdateMsg::operator=( MemberUpdateMsg const & msg){
    this->type = msg.type;
    this->node = msg.node;
    this->fileSystemRead = msg.fileSystemRead;
    return *this;   
}

void Membership::pushMsgToFileSysQueue(MemberUpdateMsg msg){
    std::unique_lock<std::mutex> locker( fileSysMsgQueueLock );

    this->fileSysMsgQueue.push_back(msg);

    this->fileSysMsgQueueCV.notify_all();
    usleep(5000);
}

MemberUpdateMsg Membership::pullMsgFromFileSysQueue(){
    std::unique_lock<std::mutex> locker( fileSysMsgQueueLock );

    while( fileSysMsgQueue.size()==0 ){
        this->fileSysMsgQueueCV.wait( locker );
    }
    
    MemberUpdateMsg msg = fileSysMsgQueue[ 0 ];
    fileSysMsgQueue.erase( fileSysMsgQueue.begin() );
    
    return msg;
}

bool Membership::emptyFileSysQueue(){
    fileSysMsgQueueLock.lock();
    bool ret = fileSysMsgQueue.size();
    fileSysMsgQueueLock.unlock();
    return ret;
}

Node Membership::getMyNode(){
    return members[0];
}