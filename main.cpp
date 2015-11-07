#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>
#include <thread>
#include <stdlib.h>     // atoi

#include "constant.h"
#include "connections.h"
#include "Membership.h"
#include "FileSystem.h"

using namespace std;

std::ofstream logFile;

/* User thread: Waits for user to input a grep command 
When receiving the grep command from command line (test cases uses this), 
it will bypass the cin*/
void listeningCin(Membership* m, FileSystem* fs)
{
    std::string input;
    while (true)
    {
        std::cout << "Type a command (help for a list): ";
        // getline(std::cin, input);
        std::cin >> input;
        //std::cout << "You entered: " << input << std::endl;

        if (input.compare("help") == 0 || input.compare("h") == 0)
        {
            std::cout << "quit      (q)     - Exit normally" << std::endl;
            std::cout << "table     (t)     - Table of Membership List" << std::endl;
            std::cout << "join      (j)     - Join the system" << std::endl;
            std::cout << "leave     (l)     - Leave the system" << std::endl;
            std::cout << "leader    (ml)    - Leave the system" << std::endl;
            std::cout << "netstat   (n)     - Network usage" << std::endl;
            std::cout << "vtable    (vt)    - List the virtual ring for the filesystem" << std::endl;
            std::cout << "put       (p)     - Insert a file (put localfile remotefile)" << std::endl;
            std::cout << "get       (g)     - Get a file  (get remotefile localfile)" << std::endl;
            std::cout << "rm        ()      - Remove a file  (rm remotefile)" << std::endl;
            std::cout << "stored    ()      - List files in the present node" << std::endl;
        }
        else if (input.compare("quit") == 0 || input.compare("q") == 0)
        {
            std::cout << "Exiting normally " << std::endl;
            break;
        }
        else if (input.compare("clear") == 0 || input.compare("c") == 0)
        {
            system("clear");
        }
        else if (input.compare("table") == 0 || input.compare("t") == 0)
        {
            std::cout << m->printMember();
        }
        else if (input.compare("leave") == 0 || input.compare("l") == 0)
        {
            m->leave();
        }
        else if (input.compare("join") == 0 || input.compare("j") == 0)
        {
            m->join();
        }
        else if (input.compare("leader") == 0 || input.compare("ml") == 0)
        {
            std::cout << m->getLeader() << std::endl;
        }
        else if (input.compare("netstat") == 0 || input.compare("n") == 0)
        {
            std::cout << "UDP Stats: Sent: " << getUDPSent();
            std::cout << " Received: " << getUDPReceived() << std::endl;
        }
        else if (input.compare("put") == 0 || input.compare("p") == 0)
        {
            std::string localfile;
            std::string remotefile;
            std::cin >> localfile;
            std::cin >> remotefile;

            fs->put(localfile, remotefile);
        }
        else if (input.compare("get") == 0 || input.compare("g") == 0)
        {
            std::string localfile;
            std::string remotefile;
            std::cin >> remotefile;
            std::cin >> localfile;

            fs->get(localfile, remotefile);
        }
        else if (input.compare("vtable") == 0 || input.compare("vt") == 0)
        {
            fs->printVirtualRing();
        }
        else if (input.compare("rm") == 0)
        {
            std::string remotefile;
            std::cin >> remotefile;
            fs->deleteFromFS(remotefile);
        }
        else if (input.compare("list") == 0)
        {
            std::string remotefile;
            std::cin >> remotefile;
            fs->where(remotefile);
        }
        else if (input.compare("stored") == 0 || input.compare("s") == 0)
        {
            std::cout << "Local Files List: " << std::endl;
            std::cout << fs->getFileList() << std::endl;
        }
        else{
            std::cout << "PLEASE CHECK AGAIN THE POSSIBLE OPTIONS" << std::endl;
        }

    }

    return;
}

int main (int argc, char* argv[])
{
    srand (time(NULL));

    system("rm files/*");
    system("clear");

    std::cout << std::endl << "CS425 - MP3: Distributed File System" ;
    std::cout << std::endl << std::endl;

    logFile.open("log.log");
    logFile << "Inicializing! " << std::endl;

    int port = atoi(argv[1]);
    bool isIntroducer = atoi(argv[2]);

    Membership m(isIntroducer, port);
    FileSystem fs (port + 21, m);

    std::thread cinListening(listeningCin, &m, &fs);
    cinListening.join();

    return 0;
}
