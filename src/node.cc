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
 * Derive the Node class from cSimpleModule. This is the class where a node can be a sender or a receiver.
 * If it's a sender then it processes the messages in the window it needs to send, sends them and awaits a corresponding ACK for each.
 * It also sets a timer (by self-messaging) to re-send the whole window again in case of the absence of awaited ACKs and times out.
 * The receiver however can only receive the message it's waiting for (identifies this by the sequence number) and send an ACK in case of
 * a message with correct parity or a NACK in case of incorrect parity.
 */
class Node : public cSimpleModule
{
  protected:
    /// Data members
    bool sender = false; // Used to indicate if a node is a sender or a receiver of the Go Back N algorithm.
    bool initial = true; // Used to receive the first initialization message of the coordinator.
    int index = 0;  // Used to store the index of the node itself. (in our case 0 or 1).
    int seqNum = 0; // Used to keep track of which messages were sent & acknowledged.
    int seqBeg = 0; // Used to indicate the sequence number of the beginning of the window.
    int stoppedTimeoutCount = 0; /* Used to keep count of the acknowledgments received from the receiver and its timer hasn't timed out yet.
    Used to avoid timing out on messages of which the sender already received an a acknowledgment.*/
    double lastTime = 0.0; // Used in scheduling the next message to send.
    int logSeqNum = -1; // Used to help in printing the log of reading the line.
    std::queue<bool> sentFlag; //Used to indicate the amount of the messages in the window that have been sent.
    std::vector<std::string> errors,messages; /* Used to store all the messages and their channel errors from the input file instead of reading the file multiple times.
    The following redefined virtual function holds the algorithm.*/
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    // Byte Stuffing algorithm. It takes the index of the message to perform the byte stuffing on.
    std::string byteStuffing(int seqNumber);
    /// File Helper Functions
    void readInputFile(const char *filename); // Used to read the input file at the sender.
    void writeOutputFile(const char *filename, std::string logMessage); // Used to write a line directly to the output file.
    std::string writeOutputFileBP(const char *filename, double startingPT, int j, bool write=true); // Used to write the output line of reading input line before processing.
    // Used to write the output line after processing the message and on sending it through the channel.
    std::string writeOutputFileBT(const char *filename, double startingTR, std::string verb, int seqNumber, std::string payload, std::string trailer, int modified, bool mod, bool lost, int duplicate, double delay, bool write=true);
    // Used to write the output line on timeout event.
    std::string writeOutputFileTO(const char *filename, double timeoutTime, int seqNumber, bool write=true);
    // Used to write the output line after processing and on sending the control frame through the channel.
    std::string writeOutputFileCF(const char *filename, double startingTR, bool nack, int ackNum, bool loss, bool write=true);
};

// The module class needs to be registered with OMNeT++
Define_Module(Node);

void Node::initialize()
{
}

void Node::handleMessage(cMessage *msg)
{
    MessageFrame_Base *mmsg = check_and_cast<MessageFrame_Base *> (msg); // Casting from the general message class to the custom made one.
    double delays =  double(getParentModule()->par("PT"))+double(getParentModule()->par("TD"));
    bool timeOut = false; // Used to indicate whether a timeout event occurs.
    bool receivedAck = false; // Used to indicate whether the correct ACK was received.
    bool noErrors = false; // Used to be able to send the first frame after a timeout error free.
    // For logging purposes.
    std::string log;
    if(mmsg->isSelfMessage() && mmsg->getFrameType() == -1){
        writeOutputFile("output.txt", mmsg->getPayload());
        EV<<mmsg->getPayload();
        cancelAndDelete(msg);
        return;
    // Check for timeouts in sender.
    } else if(mmsg->isSelfMessage() && seqNum<messages.size()){
        // Check if the timer was already stopped either by receiving an ACK or by another timeout being triggered.
        if(stoppedTimeoutCount>0)
            stoppedTimeoutCount--;
        else {
            timeOut = true;
            noErrors = true;
            log = writeOutputFileTO("output.txt", simTime().dbl(), seqNum%int(getParentModule()->par("WS")));
            EV<<log;
            while(!sentFlag.empty())
            {
                sentFlag.pop();
                stoppedTimeoutCount++; // Avoid the timeouts of the rest of the window.
            }
            stoppedTimeoutCount--;
        }
    }
    // Initialize sender and receiver settings.
    std::string receiving ="No";
    if(initial && mmsg->getPayload() == receiving){// Initialize receiver
        initial = false;
        cancelAndDelete(msg); // Release resources.
        if(isName("node0"))
            index = 0;
        else
            index = 1;
        return;
    } else if(initial) {// Initialize sender
        sender = true;
        if(isName("node0"))
            index = 0;
        else
            index = 1;
        seqBeg = 0;
        std::string fileName = "input"+std::to_string(index)+".txt";
        // Reading all the messages at one and storing them in a vector with their errors.
        // Just to avoid reading the file multiple times.
        readInputFile(fileName.c_str());
    }
    // Sender handler.
    if(sender){
        // Send messages in 3 cases: Initial state, Timeout State & Receiving the correct ACK (since we move the window).
        if(mmsg->getFrameType() == 1 || initial || timeOut){
            // Check if the received ACK is the one the sender is waiting for.
            if(!timeOut && mmsg->getAckNum() == (seqBeg+1)%int(getParentModule()->par("WS")))
            {
                // Move the window.
                seqBeg++;
                seqBeg %= (int(getParentModule()->par("WS")));
                seqNum++;
                stoppedTimeoutCount++; // Avoid its timeout/stop its timer.
                sentFlag.pop();
                receivedAck = true;
            }
            // If there are still messages the sender wants to send.
            if(seqNum<messages.size()){
                double newDelay = 0;
                if (lastTime > simTime().dbl())
                    newDelay = lastTime - simTime().dbl();
                double newTime = simTime().dbl();
                if(timeOut){
                    newDelay = 0;
                    newTime = simTime().dbl();
                    timeOut = false;
                }
                if(receivedAck){
                    newTime = simTime().dbl();
                    receivedAck = false;
                }
                // Start from the correct position in the window.
                for(int i=sentFlag.size(); i<int(getParentModule()->par("WS")); i++){
                    if(initial){// Add the starting time for the initial send.
                        newDelay += std::stod(mmsg->getPayload());
                        newTime += std::stod(mmsg->getPayload());
                        initial = false;
                    }
                    int j = seqNum + i;
                    if(j >= messages.size()) // If there are no more messages to send, exit.
                        break;
                    // Perform byte stuffing on message.
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
                    else
                        errors[j] = "0000";
                    noErrors = false;
                    // Create a new message to send.
                    MessageFrame_Base *newMsg = new MessageFrame_Base(value.c_str());
                    newMsg->setPayload(value);
                    newMsg->setSeqNum((seqBeg+i)%int(getParentModule()->par("WS")));
                    // Add Parity/Trailer.
                    bits parity(std::string("00000000"));
                    for(int i=0; i<value.size(); i++)
                    {
                        bits temp(value[i]);
                        parity = parity ^ temp;
                    }
                    newMsg->setParity(static_cast<char>( parity.to_ulong() ));
                    newMsg->setFrameType(0); // i.e. data frame.
                    newDelay += delays;
                    newTime += double(getParentModule()->par("PT"));
                    double temp = (newTime + double(getParentModule()->par("TO")));
                    // Variable to ease printing logs
                    int duplicate = 0;
                    if(duplicationE)
                        duplicate = 1;
                    int seqNumber = (seqBeg+i)%int(getParentModule()->par("WS"));
                    int errorDelay = double(getParentModule()->par("ED"));
                    std::string payload = newMsg->getPayload();
                    std::string trailer = bits(newMsg->getParity()).to_string();
                    int modifiedBitNumber = 0;
                    // Handle loss, delay, modification and duplication channel errors.
                    if(!lossE){
                        if(modificationE){
                            std::string modifiedMsg = newMsg->getPayload();
                            int randomI = int(uniform(0,modifiedMsg.size()));
                            bits modifiedBits(modifiedMsg[randomI]);
                            int randomBit = int(uniform(0,8));
                            modifiedBits[randomBit] = ~modifiedBits[randomBit];
                            modifiedMsg[randomI] = static_cast<char>( modifiedBits.to_ulong());
                            newMsg->setPayload(modifiedMsg);
                            newMsg->setName(modifiedMsg.c_str());
                            payload = modifiedMsg;
                            modifiedBitNumber = 8*randomI + randomBit;
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
                    // Just some logging to the output file.
                    if(simTime().dbl() + newDelay - delays != simTime().dbl()){
                        if(j > logSeqNum){
                            // Send a self message to write the output at its correct time.
                            std::string m;
                            m = writeOutputFileBP("output.txt", simTime().dbl() + newDelay - delays, j, false);
                            MessageFrame_Base *logMsg = new MessageFrame_Base("");
                            logMsg->setPayload(m);
                            logMsg->setFrameType(-1);
                            scheduleAt(simTime().dbl() + newDelay - delays, logMsg);
                            logSeqNum++;
                        }
                    }
                    else
                    {
                        if(j > logSeqNum){
                            log = writeOutputFileBP("output.txt", simTime().dbl() + newDelay - delays, j);
                            EV<<log;
                            logSeqNum++;
                        }
                    }
                    if(simTime().dbl() != newTime){
                        std::string m1, m2;
                        if(delayE)
                            m1 = writeOutputFileBT("output.txt", newTime, "sent", seqNumber, payload, trailer, modifiedBitNumber, modificationE, lossE, duplicate, errorDelay, false);
                        else
                            m1 = writeOutputFileBT("output.txt", newTime, "sent", seqNumber, payload, trailer, modifiedBitNumber, modificationE, lossE, duplicate, 0.0, false);
                        MessageFrame_Base *logMsg1 = new MessageFrame_Base("");
                        logMsg1->setPayload(m1);
                        logMsg1->setFrameType(-1);
                        scheduleAt(newTime, logMsg1);
                        if(delayE && duplicationE)
                            m2 = writeOutputFileBT("output.txt", newTime+double(getParentModule()->par("DD")), "sent", seqNumber, payload, trailer, modifiedBitNumber, modificationE, lossE, duplicate+1, errorDelay, false);
                        else if(duplicationE)
                            m2 = writeOutputFileBT("output.txt", newTime+double(getParentModule()->par("DD")), "sent", seqNumber, payload, trailer, modifiedBitNumber, modificationE, lossE, duplicate+1, 0.0, false);
                        if(duplicationE){
                            MessageFrame_Base *logMsg2 = new MessageFrame_Base("");
                            logMsg2->setPayload(m2);
                            logMsg2->setFrameType(-1);
                            scheduleAt(newTime+double(getParentModule()->par("DD")), logMsg2);
                        }
                    } else {
                        if(delayE)
                            log = writeOutputFileBT("output.txt", newTime, "sent", seqNumber, payload, trailer, modifiedBitNumber, modificationE, lossE, duplicate, errorDelay);
                        else
                            log = writeOutputFileBT("output.txt", newTime, "sent", seqNumber, payload, trailer, modifiedBitNumber, modificationE, lossE, duplicate, 0.0);
                        EV<<log;
                        if(delayE && duplicationE)
                            log = writeOutputFileBT("output.txt", newTime+double(getParentModule()->par("DD")), "sent", seqNumber, payload, trailer, modifiedBitNumber, modificationE, lossE, duplicate+1, errorDelay);
                        else if(duplicationE)
                            log = writeOutputFileBT("output.txt", newTime+double(getParentModule()->par("DD")), "sent", seqNumber, payload, trailer, modifiedBitNumber, modificationE, lossE, duplicate+1, 0.0);
                        EV<<log;
                    }
                    // Start Timer by self messaging.
                    MessageFrame_Base *timerMsg = new MessageFrame_Base("Timeout");
                    timerMsg->setSeqNum((seqBeg+i)%int(getParentModule()->par("WS")));
                    scheduleAt(newTime + double(getParentModule()->par("TO")), timerMsg);
                    sentFlag.push(true);
                    newDelay -= double(getParentModule()->par("TD"));
                }
                lastTime = newDelay + simTime().dbl();
            }
        }
    // Receiver Handler
    } else {
        // Check if the received message is the awaited one.
        if(mmsg->getSeqNum() == seqNum){
                bool ackLost = false;
                int randomOccurance = int(uniform(0,100));
                if((randomOccurance+1)/100.0 <= double(getParentModule()->par("LP")))
                    ackLost = true;
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
                // Check if the received message has a correct parity.
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
                // Check if the ACK/NACK was lost and log it accordingly in the output file.
                if(!ackLost){
                    sendDelayed(ackMsg, newDelay,"nodeGate$o"); // send out the message
                    std::string m;
                    m = writeOutputFileCF("output.txt", simTime().dbl() + double(getParentModule()->par("PT")), !sendack, ackMsg->getAckNum(), ackLost, false);
                    MessageFrame_Base *logMsg = new MessageFrame_Base("");
                    logMsg->setPayload(m);
                    logMsg->setFrameType(-1);
                    scheduleAt(simTime().dbl() + double(getParentModule()->par("PT")), logMsg);
                }
                else{
                    std::string m;
                    m = writeOutputFileCF("output.txt", simTime().dbl() + double(getParentModule()->par("PT")), !sendack, ackMsg->getAckNum(), ackLost, false);
                    MessageFrame_Base *logMsg = new MessageFrame_Base("");
                    logMsg->setPayload(m);
                    logMsg->setFrameType(-1);
                    scheduleAt(simTime().dbl() + double(getParentModule()->par("PT")), logMsg);
                    cancelAndDelete(ackMsg);
                    if(sendack){
                        seqNum--;
                        seqNum += int(getParentModule()->par("WS"));
                        seqNum %= int(getParentModule()->par("WS"));
                    }
                }
        }
    }
    cancelAndDelete(msg); // Release the resources of the message received.
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
    filestream.close();
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

void Node::writeOutputFile(const char *filename, std::string logMessage){
    std::ofstream filestream;
    filestream.open(filename, std::ios_base::app);
    if(!filestream) {
        throw cRuntimeError("Error opening file '%s'?", filename);
    } else {
        filestream.write(logMessage.c_str(), logMessage.size());
    }
    filestream.close();
    return;
}

// At time [.. starting processing time….. ], Node[id] , Introducing channel error with code=[ …code in 4 bits… ] .
std::string Node::writeOutputFileBP(const char *filename, double startingPT, int j, bool write)
{
    std::ofstream filestream;
    std::string line = "At time ["+std::to_string(int(startingPT));
    if(int((startingPT-int(startingPT))*10) != 0)
        line += "."+std::to_string(int((startingPT-int(startingPT))*10));
    line += "], Node["+std::to_string(index)+"] , Introducing channel error with code =["+errors[j]+"]\n";
    filestream.open(filename, std::ios_base::app);
    if(!filestream) {
        throw cRuntimeError("Error opening file '%s'?", filename);
    } else {
        if(write)
            filestream.write(line.c_str(), line.size());
    }
    filestream.close();
    return line;
}

//At time [.. starting sending time after processing….. ], Node[id] [sent/received] frame with seq_num=[..] and payload=[ ….. in characters after modification….. ] and trailer=[…….in bits….. ] ,
//Modified [-1 for no modification, otherwise the modified bit number] ,Lost [Yes/No], Duplicate [0 for none, 1 for the first version, 2 for the second version], Delay [0 for no delay , otherwise the error delay interval].
std::string Node::writeOutputFileBT(const char *filename, double startingTR, std::string verb, int seqNumber, std::string payload, std::string trailer, int modified, bool mod, bool lost, int duplicate, double delay, bool write){
    std::ofstream filestream;
    std::string line = "At time ["+std::to_string(int(startingTR));
    if(int((startingTR-int(startingTR))*10) != 0)
        line += "."+std::to_string(int((startingTR-int(startingTR))*10));
    line += "], Node["+std::to_string(index)+"] ["+verb+"] frame with ";
    line += "seq_num=["+std::to_string(seqNumber)+"] and payload=["+payload+"] and trailer=["+trailer+"] , Modified [";
    if(!mod)
        line +="-1";
    else
        line += std::to_string(modified);
    line +="] ,Lost [";
    if(lost)
        line+= "Yes";
    else
        line+= "No";
    if(sender){
        line += "], Duplicate ["+std::to_string(duplicate)+"], Delay ["+std::to_string(int(delay));
        if(int((delay-int(delay))*10) != 0)
            line += "."+std::to_string(int((delay-int(delay))*10));
        line += "]\n";
    }
    else
        line += "]\n";
    filestream.open(filename, std::ios_base::app);
    if(!filestream) {
        throw cRuntimeError("Error opening file '%s'?", filename);
    } else {
        if(write)
            filestream.write(line.c_str(), line.size());
    }
    filestream.close();
    return line;
}

//Time out event at time [.. timer off-time….. ], at Node[id] for frame with seq_num=[..]
std::string Node::writeOutputFileTO(const char *filename, double timeoutTime, int seqNumber, bool write){
    std::ofstream filestream;
    std::string line = "Time out event at time ["+std::to_string(int(timeoutTime));
    if(int((timeoutTime-int(timeoutTime))*10) != 0)
        line += "."+std::to_string(int((timeoutTime-int(timeoutTime))*10));
    line += "], at Node["+std::to_string(index)+"] for frame with seq_num=["+std::to_string(seqNumber)+"]\n";
    filestream.open(filename, std::ios_base::app);
    if(!filestream) {
        throw cRuntimeError("Error opening file '%s'?", filename);
    } else {
        if(write)
            filestream.write(line.c_str(), line.size());
    }
    filestream.close();
    return line;
}

//At time[.. starting sending time after processing….. ], Node[id] Sending [ACK/NACK] with number […] , loss [Yes/No ]
std::string Node::writeOutputFileCF(const char *filename, double startingTR, bool nack, int ackNum, bool loss, bool write){
    std::ofstream filestream;
    std::string line = "At time ["+std::to_string(int(startingTR));
    if(int((startingTR-int(startingTR))*10) != 0)
        line += "."+std::to_string(int((startingTR-int(startingTR))*10));
    line += "], Node["+std::to_string(index)+"] Sending [";
    if(nack)
        line += "NACK";
    else
        line += "ACK";
    line += "] with number ["+std::to_string(ackNum)+"] , loss [";
    if(loss)
        line += "Yes]\n";
    else
        line += "No]\n";
    filestream.open(filename, std::ios_base::app);
    if(!filestream) {
        throw cRuntimeError("Error opening file '%s'?", filename);
    } else {
        if(write)
            filestream.write(line.c_str(), line.size());
    }
    filestream.close();
    return line;
}
