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
#include <queue>
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
    int seqBeg = 0;
    int seqEnd = 0;
    int ackIndex;
    double lastTime = 0.0;
//    int seqValue = 0;
    std::queue<bool> sentFlag;
    std::vector<std::string> errors,messages;

    // The following redefined virtual function holds the algorithm.
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    void readInputFile(const char *filename);
    std::string byteStuffing(int seqNumber);
};

// The module class needs to be registered with OMNeT++
Define_Module(Node);

void Node::initialize()
{
}

void Node::handleMessage(cMessage *msg)
{
    MessageFrame_Base *mmsg = check_and_cast<MessageFrame_Base *> (msg);
    double delays =  double(getParentModule()->par("PT"))+double(getParentModule()->par("TD"));
    if(mmsg->isSelfMessage()){
//        msg->setName("Hello from Hub");
//        int Chosen = int(uniform(0,int(getParentModule()->par("N"))));
//        double interval = exponential(2.0);
//        scheduleAt(simTime() + interval, new cMessage(""));
//        send(msg, "port$o", Chosen);
    }
    else {
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
        std::string receiving ="No";
        if(initial && mmsg->getPayload() == receiving){
            initial = false;
            cancelAndDelete(msg);
            return;
        } else if(initial) {
            sender = true;
            if(isName("node0"))
                index = 0;
            else
                index = 1;
            seqBeg = 0;
            seqEnd = int(getParentModule()->par("WS"))-1;
            ackIndex = int(getParentModule()->par("WS"));
//            delays += std::stod(mmsg->getPayload());
            std::string fileName = "input"+std::to_string(index)+".txt";
            readInputFile(fileName.c_str());
//            initial = false;
        }
        if(sender){
            if(mmsg->getFrameType() == 1 || initial){
                if(mmsg->getAckNum() == seqBeg+1)
                {

                    seqBeg++;
                    seqBeg %= (int(getParentModule()->par("WS"))+1);
                    seqEnd++;
                    seqEnd %= (int(getParentModule()->par("WS"))+1);
                    seqNum++;
//                    seqNum %= (int(getParentModule()->par("WS"))+1);
                    sentFlag.pop();
                }
                if(seqNum<messages.size()){
                    double newDelay = lastTime - simTime().dbl();
//                    double newDelay = 0.0;
//                    for(int i=(seqBeg+sentFlag.size())%(int(getParentModule()->par("WS"))+1); i<=seqEnd+seqNum; i++){
                    for(int i=sentFlag.size(); i<int(getParentModule()->par("WS")); i++){
                        if(initial){
                            newDelay += std::stod(mmsg->getPayload());
//                            EV<<"Starting Time: ";
//                            EV<<std::stod(mmsg->getPayload());
                            initial = false;
                        }
                        EV<<"\ni= ";
                        EV<<i;
                        int j = seqNum + i;
                        EV<<"\nj= ";
                        EV<<j;
                        if(j >= messages.size())
                            break;
                        std::string value = byteStuffing(j);
                        MessageFrame_Base *newMsg = new MessageFrame_Base(value.c_str());
                        newMsg->setSeqNum((seqBeg+i)%(int(getParentModule()->par("WS"))+1));
                        bits parity(std::string("00000000"));
                        for(int i=0; i<value.size(); i++)
                        {
                            bits temp(value[i]);
                            parity = parity ^ temp;
                        }
                        newMsg->setParity(static_cast<char>( parity.to_ulong() ));
                        newMsg->setFrameType(0);
                        newDelay += delays;
                        EV<<"Delay: ";
                        EV<<newDelay;
                        sendDelayed(newMsg, newDelay, "nodeGate$o"); // send out the message
                        sentFlag.push(true);
                    }
                    lastTime = newDelay + simTime().dbl();
                }
            }
            // Receiver Handler
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
            ackMsg->setAckNum((mmsg->getSeqNum()+1)% (int(getParentModule()->par("WS"))+1));
            ackMsg->setFrameType(frameType);
//            EV<<"SimTime: ";
//            EV<<simTime().dbl();
//            double newDelay = simTime().dbl() + delays;
            double newDelay = delays;
            EV<<"NewDelay: ";
            EV<<newDelay;
            sendDelayed(ackMsg, newDelay,"nodeGate$o"); // send out the message
        }
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

std::string Node::byteStuffing(int seqNumber){
    std::string s = "$";
    for(int i=0; i<messages[seqNumber].size(); i++)
    {
        if(messages[seqNumber][i] == flag || messages[seqNumber][i] == escape)
        {
            s += escape;
        }
        s += messages[seqNumber][i];
    }
    s += flag;
    return s;
}
