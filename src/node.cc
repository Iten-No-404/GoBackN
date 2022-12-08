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
    int stoppedTimeoutCount = 0;
    double lastTime = 0.0;
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
    bool timeOut = false;
    bool receivedAck = false;
    bool noErrors = false;
    // Check for timeouts in sender.
    if(mmsg->isSelfMessage() && seqNum<messages.size()){
        // Check if the timer was already stopped.
        EV<<"stoppedTimeoutCount: ";
        EV<<stoppedTimeoutCount;
        if(stoppedTimeoutCount>0)
            stoppedTimeoutCount--;
        else {
            EV<<"Timeout!!!";
            timeOut = true;
            noErrors = true;
            while(!sentFlag.empty())
            {
                sentFlag.pop();
                stoppedTimeoutCount++;
            }
            stoppedTimeoutCount--;
        }
    }
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
    // Initialize sender and receiver settings.
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
        std::string fileName = "input"+std::to_string(index)+".txt";
        readInputFile(fileName.c_str());
    }
    // Sender handler.
    if(sender){
        // Send messages in 3 cases: Initial state, Timeout State & Receiving the correct ACK(since we move the window).
        if(mmsg->getFrameType() == 1 || initial || timeOut){
            // Check if the received ACK is the one the sender is waiting for.
            if(!timeOut && mmsg->getAckNum() == (seqBeg+1)%int(getParentModule()->par("WS")))
            {
                seqBeg++;
                seqBeg %= (int(getParentModule()->par("WS")));
                seqEnd++;
                seqEnd %= (int(getParentModule()->par("WS")));
                seqNum++;
                stoppedTimeoutCount++;
                sentFlag.pop();
                receivedAck = true;
            }
            if(seqNum<messages.size()){
                double newDelay = 0;
                if (lastTime - simTime().dbl() > simTime().dbl())
                    newDelay = lastTime - simTime().dbl();
                else
                    newDelay = 0;
                double newTime = simTime().dbl();;
                if(timeOut){
                    newDelay = 0;
                    newTime = simTime().dbl();
                    timeOut = false;
                }
                if(receivedAck){
                    newDelay = lastTime - simTime().dbl();
                    newTime = simTime().dbl();
                    receivedAck = false;
                }
                for(int i=sentFlag.size(); i<int(getParentModule()->par("WS")); i++){
                    if(initial){
                        newDelay += std::stod(mmsg->getPayload());
                        newTime += std::stod(mmsg->getPayload());
                        initial = false;
                    }
                    int j = seqNum + i;
                    if(j >= messages.size())
                        break;
                    std::string value = byteStuffing(j);
                    bool modificationE = false;
                    bool lossE = false;
                    bool duplicationE = false;
                    bool delayE = false;
                    // In case of timeout, send the first message in the window error free while the other messages with their errors.
                    if(!noErrors){
                        if(errors[j][0] == '1')
                            modificationE = true;
                        if(errors[j][1] == '1')
                            lossE = true;
                        if(errors[j][2] == '1')
                            duplicationE = true;
                        if(errors[j][3] == '1')
                            delayE = true;
                    }
                    noErrors = false;
                    MessageFrame_Base *newMsg = new MessageFrame_Base(value.c_str());
                    newMsg->setPayload(value);
                    newMsg->setSeqNum((seqBeg+i)%int(getParentModule()->par("WS")));
                    bits parity(std::string("00000000"));
                    for(int i=0; i<value.size(); i++)
                    {
                        bits temp(value[i]);
                        parity = parity ^ temp;
                    }
                    newMsg->setParity(static_cast<char>( parity.to_ulong() ));
                    newMsg->setFrameType(0);
                    newDelay += delays;
                    newTime += double(getParentModule()->par("PT"));
                    EV<<"\nDelay: ";
                    EV<<newDelay;
                    EV<<"\nSchedule At: ";
                    double temp = (newTime + double(getParentModule()->par("TO")));
                    EV<<temp;
                    if(!lossE){
                        if(modificationE){
                            std::string modifiedMsg = newMsg->getPayload();
                            int randomI = int(uniform(0,modifiedMsg.size()));
                            modifiedMsg[randomI] = ~(modifiedMsg[randomI]);
                            newMsg->setPayload(modifiedMsg);
                            newMsg->setName(modifiedMsg.c_str());
                        }
                        if(delayE)
                            sendDelayed(newMsg, newDelay + double(getParentModule()->par("ED")), "nodeGate$o");
                        else
                            sendDelayed(newMsg, newDelay, "nodeGate$o"); // send out the message
                        if(delayE && duplicationE)
                            sendDelayed(newMsg->dup(), newDelay + double(getParentModule()->par("ED")) + double(getParentModule()->par("DD")), "nodeGate$o");
                        else if(duplicationE)
                            sendDelayed(newMsg->dup(), newDelay + double(getParentModule()->par("DD")), "nodeGate$o"); // send out the message
                    }
                    else
                        cancelAndDelete(newMsg); // If the message was lost, clear its resources.
                    // Start Timer
                    MessageFrame_Base *timerMsg = new MessageFrame_Base("Timeout");
                    timerMsg->setSeqNum((seqBeg+i)%int(getParentModule()->par("WS")));
                    scheduleAt(newTime + double(getParentModule()->par("TO")), timerMsg);
                    sentFlag.push(true);
                }
                lastTime = newDelay + simTime().dbl();
            }
        }
    // Receiver Handler
    } else {
        if(mmsg->getSeqNum() == seqNum){
                bool ackLost = false;
                int randomOccurance = int(uniform(0,100));
                if((randomOccurance+1)/100.0 <= double(getParentModule()->par("LP")))
                    ackLost = true;
                EV<<"\nackLost: ";
                EV<<ackLost;
                seqNum++;
                seqNum %= int(getParentModule()->par("WS"));
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
                bool sendack = static_cast<char>( parity.to_ulong() ) == mmsg->getParity();
                if(sendack)
                {
                    noError = true;
                    name = "ACK";
                    frameType = 1;
                } else {
                    noError = false;
                    name = "NACK";
                    frameType = 2;
                    seqNum--;
                    seqNum += int(getParentModule()->par("WS"));
                    seqNum %= int(getParentModule()->par("WS"));
                }
                MessageFrame_Base *ackMsg = new MessageFrame_Base(name.c_str());
                ackMsg->setAckNum((mmsg->getSeqNum()+1)%int(getParentModule()->par("WS")));
                ackMsg->setFrameType(frameType);
                double newDelay = delays;
                EV<<"NewDelay: ";
                EV<<newDelay;
                if(!ackLost)
                    sendDelayed(ackMsg, newDelay,"nodeGate$o"); // send out the message
                else{
                    cancelAndDelete(ackMsg);
                    if(sendack){
                        seqNum--;
                        seqNum += int(getParentModule()->par("WS"));
                        seqNum %= int(getParentModule()->par("WS"));
                    }
                }
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
                std::string err = line.substr(0,4);
                errors.push_back(err);
                std::string mes = line.substr(5);
                messages.push_back(mes);
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
