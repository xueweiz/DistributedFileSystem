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

using namespace std;

std::ofstream logFile;

/* User thread: Waits for user to input a grep command 
When receiving the grep command from command line (test cases uses this), 
it will bypass the cin*/
void listeningCin(Membership* m)
{
    std::string input;
    while (true)
    {
        std::cout << "Type a command (table, leave, join or quit): ";
        getline(std::cin, input);
        //std::cout << "You entered: " << input << std::endl;

        if (input.compare("quit") == 0 || input.compare("q") == 0)
        {
            std::cout << "Exiting normally " << std::endl;
            break;
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
        else{
            std::cout << "PLEASE CHECK AGAIN THE POSSIBLE OPTIONS" << std::endl;
        }

    }

    return;
}

int main (int argc, char* argv[])
{
    srand (time(NULL));
    std::cout << std::endl << "CS425 - MP2: Membership Protocol." ;
    std::cout << std::endl << std::endl;

    logFile.open("log.log");
    logFile << "Inicializing! " << std::endl;

    int port = atoi(argv[1]);
    bool isIntroducer = atoi(argv[2]);

    Membership m(isIntroducer, port);

    std::thread cinListening(listeningCin, &m);
    cinListening.join();

    return 0;
}
