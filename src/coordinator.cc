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
 *
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
