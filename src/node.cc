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
#include <bitset>
#include "MessageFrame_m.h"
typedef std::bitset<8> bits;

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
    MessageFrame_Base *mmsg = check_and_cast<MessageFrame_Base *> (msg);
    EV<<mmsg->getFrameType();
    if(!sender){
        EV<<"\nSeqNo: ";
        EV<<mmsg->getSeqNum();
        EV<<"\nPayload: ";
        EV<<mmsg->getPayload();
        EV<<"\nParity: ";
        EV<<bits(mmsg->getParity()).to_string();
    }
    else {
        EV<<"\nAckNo: ";
        EV<<mmsg->getAckNum();
    }
    std::string sending ="Yes";
    if(initial && mmsg->getPayload() == sending){
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
        cancelAndDelete(msg);
        return;
    }
    if(sender){
        if(seqNum<messages.size()){
            std::string value = byteStuffing();
            MessageFrame_Base *newMsg = new MessageFrame_Base(value.c_str());
            newMsg->setSeqNum(seqNum);
            bits parity(std::string("00000000"));
            for(int i=0; i<value.size(); i++)
            {
                bits temp(value[i]);
                parity = parity ^ temp;
            }
            newMsg->setParity(static_cast<char>( parity.to_ulong() ));
            newMsg->setFrameType(0);
            send(newMsg, "nodeGate$o"); // send out the message
            seqNum++;
        }
    } else {
        std::string name = "";
        std::string payload = mmsg->getPayload();
        bool noError = false;
        int frameType = 2;
        bits parity(std::string("00000000"));
        for(int i=0; i<payload.size(); i++)
        {
            bits temp(payload[i]);
            parity = parity ^ temp;
        }
        if(static_cast<char>( parity.to_ulong() ) == mmsg->getParity())
        {
            noError = true;
            name = "ACK";
            frameType = 1;
        } else {
            noError = false;
            name = "NACK";
            frameType = 2;
        }
        MessageFrame_Base *ackMsg = new MessageFrame_Base(name.c_str());
        ackMsg->setAckNum(mmsg->getSeqNum()+1);
        ackMsg->setFrameType(frameType);
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
