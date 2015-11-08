/*******************************************************************************
 *  @author             Karl Jansen (kmjansen@uw.edu)
 *  @version            1.1, 06/09/2015
 *
 *  Compilation:        N/A
 *  Execution:          N/A
 *  Dependencies:       Socket.h; UdpMulticast.h;
 *
 *  Description:
 *  The UdpRelay class represents a Relay program using UDP sockets locally and
 *  TCP sockets externally to communicate with other hosts.
 *
 *  Functionality:
 *  The UdpRelay program uses a command line interface to accept user input.
 *  It accepts connections from remote hosts, and relays messages to and from
 *  the hosts that are connected to it.
 *
*******************************************************************************/

#ifndef UDPRELAY_H
#define UDPRELAY_H

#define TCP_PORT 50249                 // Port for TCP Socket Connection
#define COMMAND_MAX_SIZE 32            // Maximum size of a command
#define RAW_ADDR_SIZE 4                // Maximum size of a raw ip address
#define BUFFER_MAX_SIZE 1024           // Maximum size of a buffer

#include "Socket.h"                    // Socket
#include "UdpMulticast.h"              // UdpMulticast
#include <arpa/inet.h>                 // inet_addr
#include <iostream>                    // cerr
#include <limits.h>                    // INT_MAX
#include <map>                         // map
#include <pthread.h>                   // pthread
#include <stdlib.h>                    // atoi
#include <string>                      // string
#include <string.h>                    // strcmp

/*******************************************************************************
 * Defines the members of a UdpRelay object.
 */
class UdpRelay {
   public:
      UdpRelay(char*);
   
      static void* acceptsThread(void*);
      static void* commandThread(void*);
      static void* relayInThread(void*);
      static void* relayOutThread(void*);
   
      void commandAdd(char*, int);
      void commandDelete(char*);
      void commandHelp();
      void commandInvalid(char*);
      void commandQuit();
      void commandShow();

      pthread_cond_t* cond;
      Socket* localSocket;
      UdpMulticast* localUdpMulticast;
      char* msgBuffer;
      pthread_mutex_t* msgMutex;
      bool udpRelayRunning;

   private:
      UdpRelay();
   
      void addGrpIpToMsgHead();
      char* addrToHost(const char*);
      void bytesToAddr(const char*, char*);
      bool checkMsgForGrpIp();
      void createThreads();
      char* hostaddrToBytes(const char*);
      void joinThreads();
      void sendMsgToRemoteGrp();
      void printMsgBuffer();
   
      static pthread_t acceptsT;
      static pthread_t commandT;
      static pthread_t relayInT;
   
      char* groupIp;
      unsigned short int groupPort;
      map<string,pthread_t*> relayOutThreads;
      map<string,int> acceptedConnections;
      map<string,int> addedConnections;

};

/*******************************************************************************
 * Defines a set of parameters to be passed to a relayOutThread.
 */
class RelayOutParam {
   public:
      RelayOutParam(UdpRelay* self, char* addr, int sd):
         self(self),
         addr(addr),
         sd(sd) {};
      UdpRelay* self;                  // a pointer to the UdpRelay object
      char* addr;                      // a c-string for the peer address
      int sd;                          // the socket descriptor
};

#endif
