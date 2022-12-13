/*
 * cooridinator.cc
 *
 *  Created on: Dec 4, 2022
 *
 */

#include <string>
#include <omnetpp.h>
#include <fstream>
#include "MessageFrame_m.h"

using namespace omnetpp;

/**
 * Derive the Coordinator class from cSimpleModule. The Coordinator mainly reads the coordinator.txt to start the network.
 * It reads the starting node and the starting time. It sends a message to the starting node with the starting time given in the message.
 * It sends a message "No" to the other node to let it know that it's a receiver.
 */
class Coordinator : public cSimpleModule
{
  protected:
    // The following redefined virtual function holds the algorithm.
    char chosen;
    double startingTime;
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    char readInputFile(const char *filename);
};

// The module class needs to be registered with OMNeT++
Define_Module(Coordinator);

void Coordinator::initialize()
{
    std::ifstream OpenFile;
    char sender = '0';
    char receiver = '1';
    char content = readInputFile("coordinator.txt");
    MessageFrame_Base *senderMsg = new MessageFrame_Base(std::to_string(startingTime).c_str());
    MessageFrame_Base *receiverMsg = new MessageFrame_Base("No");
    std::string string1 = "port0$o";
    std::string string2 = "port1$o";
    if(content == '0'){
        send(senderMsg, string1.c_str());
        send(receiverMsg, string2.c_str());
    } else{
        send(senderMsg, string2.c_str());
        send(receiverMsg, string1.c_str());
    }
    std::ofstream filestream;
    filestream.open("output.txt", std::ofstream::out);
    if(!filestream)
        throw cRuntimeError("Error opening file '%s'?", "output.txt");
    else
        filestream.close();
}

char Coordinator::readInputFile(const char *filename)
{
    std::ifstream filestream;
    std::string line;

    filestream.open(filename, std::ifstream::in);

    if(!filestream) {
        throw cRuntimeError("Error opening file '%s'?", filename);
        return -1;
    } else {
        while ( getline(filestream, line) ) {
//                EV<<line;
            if (line.find(',')) {
                int beg = line.find(',');
                chosen = line[beg-1];
                startingTime = std::stod(line.substr(beg+1, line.size()-beg-2));
//                EV<<chosen;
                return chosen;

            }
        }
    }
    return '_';
}

void Coordinator::handleMessage(cMessage *msg)
{

}
