package cnproject;

@license(LGPL);
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//
simple Node
{
    gates:
        inout nodeGate;
        inout nodeCoord;
}
simple Coordinator
{
    gates:
        inout port0;
        inout port1;
}
//
// This network represents a dynamic star topology.
// A Star network topology consists of one Hub and N nodes.
// N should be configurable in the .ini file.
// Each node is connected only to the hub.
// The hub starts by send to a random node m a message “Hello from Hub”.
// The node m should respond with “Hi from node” to the Hub.
// After a random time, the Hub selects another random node m to send the same message to it.
// The node responds with the same response …. etc.
// Before starting, every node prints to the console a message “ Starting Node i”, where i is the node index. 
// 
network Network
{
    parameters:
        @display("bgb=859,220");
    submodules:
        coordinator: Coordinator {
            @display("p=352,134");
        }
        node0: Node {
            @display("p=597,63");
        }
        node1: Node {
            @display("p=87,63");
        }
    connections:
        node0.nodeGate <--> node1.nodeGate;
        node0.nodeCoord <--> coordinator.port0;
        node1.nodeCoord <--> coordinator.port1;
}
