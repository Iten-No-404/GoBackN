/*
 * node.cc
 *
 *  Created on: Dec 4, 2022
 *
 */
#include <string>
#include <omnetpp.h>
#include <fstream>
#include <vector>
#include "MessageFrame_m.h"

#define flag '$'
#define escape '/'
using namespace omnetpp;

/**
 * Derive the Node class from cSimpleModule. This is the class where a message is sent as a reply to each
 * message received from the hub.
 */
class Node : public cSimpleModule
{
  protected:
    // data members
    bool sender = false;
    bool initial = true;
    int index = 0;
    int seqNum = 0;
    std::vector<std::string> errors,messages;
    // The following redefined virtual function holds the algorithm.
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    void readInputFile(const char *filename);
    std::string byteStuffing();
};

// The module class needs to be registered with OMNeT++
Define_Module(Node);

void Node::initialize()
{
}

void Node::handleMessage(cMessage *msg)
{
//    EV<<msg->getFullName();
    MessageFrame_Base *mmsg = check_and_cast<MessageFrame_Base *> (msg);
    EV<<mmsg->getPayload();
    std::string sending ="Yes";
    if(initial && mmsg->getPayload() == sending){
//    if(initial && msg->getFullName() == sending){
//        EV<<"Entered!";
        sender = true;
        if(isName("node0"))
            index = 0;
        else
            index = 1;
        std::string fileName = "input"+std::to_string(index)+".txt";
        readInputFile(fileName.c_str());
        initial = false;
    } else if(initial) {
        initial = false;
        return;
    }
    if(sender){
        if(seqNum<messages.size()){
            std::string value = byteStuffing();
//            cMessage *newMsg = new cMessage(" ");
            MessageFrame_Base *newMsg = new MessageFrame_Base(value.c_str());
//            newMsg->setName(value.c_str());
//            newMsg->setPayload(value);
            send(newMsg, "nodeGate$o"); // send out the message
            seqNum++;
        }
    } else {
//        cMessage *ackMsg = new cMessage("ACK");
        MessageFrame_Base *ackMsg = new MessageFrame_Base("ACK");
        send(ackMsg, "nodeGate$o"); // send out the message
    }
    cancelAndDelete(msg);
}

void Node::readInputFile(const char *filename)
{
    std::ifstream filestream;
    std::string line;

    filestream.open(filename, std::ifstream::in);

    if(!filestream) {
        throw cRuntimeError("Error opening file '%s'?", filename);
    } else {
        while ( getline(filestream, line) ) {
            if (line.find('#') == 0) {
                continue; // ignore comment lines
            }
            else {
//                EV<<line;
//                EV<<"\n";
                std::string err = line.substr(0,4);
                errors.push_back(err);
                std::string mes = line.substr(5);
                messages.push_back(mes);
//                EV<<err;
//                EV<<"\n";
//                EV<<mes;
//                EV<<"\n";
//                return line[line.size()-1];
            }
        }
    }
    return;
}

std::string Node::byteStuffing(){
    std::string s = "$";
    for(int i=0; i<messages[seqNum].size(); i++)
    {
        if(messages[seqNum][i] == flag || messages[seqNum][i] == escape)
        {
            s += escape;
        }
        s += messages[seqNum][i];
    }
    s += flag;
    return s;
}
