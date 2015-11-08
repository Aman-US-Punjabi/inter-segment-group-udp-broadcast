/*******************************************************************************
 *  @author             Karl Jansen (kmjansen@uw.edu)
 *  @version            1.1, 06/09/2015
 *
 *  Compilation:        $> g++ Socket.cpp UdpMulticast.cpp UdpRelay.cpp driver.cpp -o UdpRelay -lpthread
 *  Execution:          $> ./UdpRelay ###.###.###.###:#####
 *  Dependencies:       UdpRelay.h;
 *
 *  Purpose:
 *  This is an implementation of the UdpRelay class. This class enables the
 *  communication of network messages to and from remote network groups and
 *  within local network groups.
 *
 *  Functionality:
 *  This class implements the interface specified for the UdpRelay class. When
 *  instantiated the object creates 3 persistent threads, one of which accepts
 *  commands from the user via standard input. The threads do work, including
 *  accepting socket connections with a remote network via TCP, processing
 *  a message received via a socket from either a remote network or the local
 *  network, and sending the message to either a remote network or the local
 *  network.
 *
 *  Assumptions:
 *  This class assumes a constant port will be used for the TCP connections to
 *  a remote network, namely TCP_PORT. Also, commands entered by the user which
 *  include additional parameters must be entered in the correct format and with
 *  valid values as limited checks are available for these inputs. 
 *
*******************************************************************************/

#include "UdpRelay.h"

// Define static members
pthread_t UdpRelay::acceptsT;
pthread_t UdpRelay::commandT;
pthread_t UdpRelay::relayInT;

/*******************************************************************************
 * Default Constructor
 * This is private, and cannot be accessed by a client using this class.
 */
UdpRelay::UdpRelay() {
   // Empty
}

/*******************************************************************************
 * Overloaded Constructor
 * This is public, and it initializes a new UdpRelay object with the given 
 * address parameter. This object instansiates a Socket object and a UdpMulicast
 * object to facilitate communications with local and remote hosts. Private
 * helper functions spin off instance threads and join them when they are done
 * being used.
 * @param udpPort       a character string representing the UDP port number
 *                      the format expected is ###.###.###.###:#####
 */
UdpRelay::UdpRelay(char* udpAddr) {
   // Parse parameter and assign members
   groupIp = strtok(udpAddr, ":");
   groupPort = (unsigned short int)atoi(strtok(NULL, "\0"));
   udpRelayRunning = true;
   // Initialize dynamic members
   localSocket = new Socket(TCP_PORT);
   localUdpMulticast = new UdpMulticast(groupIp, groupPort);
   msgBuffer = new char[BUFFER_MAX_SIZE];
   msgMutex = new pthread_mutex_t;
   cond = new pthread_cond_t;
   // Print welcome message
   printf("> booted up at %s:%d\n", groupIp, groupPort);
   // Create persistent threads
   createThreads();
   // Join persistent threads
   joinThreads();
   // Clean up dynamic members
   delete localSocket;
   localSocket = NULL;
   delete localUdpMulticast;
   localUdpMulticast = NULL;
   delete msgBuffer;
   msgBuffer = NULL;
   delete msgMutex;
   msgMutex = NULL;
   delete cond;
   cond = NULL;
}

/*******************************************************************************
 * Accepts Thread
 * Uses the class's Socket object to receive TCP connections to remote network
 * groups. When a connection is established, a relayOutThread is started. When
 * a connection already exists and another one is requested from the same
 * network group, the old connection is closed and the associated relayOutThread
 * is cancelled and a new connection is created with a new relayOutThread.
 * @param arg           a pointer to the this thread's parent, the UdpRelay object
 */
void* UdpRelay::acceptsThread(void* arg) {
   UdpRelay* parent = (UdpRelay*)arg;
   while (true) {
      // Accept a connection request from a remote relay
      int serverTcpSd = parent->localSocket->getServerSocket();
      // Retrieve hostname and ipaddress (NOTE: this code is duplicated in commandAdd)
      struct sockaddr_storage sockAddrStorage;
      memset(&sockAddrStorage, 0, sizeof(sockAddrStorage));
      struct sockaddr_in sockAddrIn;
      memset(&sockAddrIn, 0, sizeof(sockAddrIn));
      char ipAddrStr[INET_ADDRSTRLEN];
      socklen_t len = sizeof(sockAddrStorage);
      getpeername(serverTcpSd, (struct sockaddr*)&sockAddrStorage, &len);
      struct sockaddr_in* sockAddrInPtr = (struct sockaddr_in*)&sockAddrStorage;
      unsigned short int port = ntohs(sockAddrInPtr->sin_port);
      inet_ntop(AF_INET, &sockAddrInPtr->sin_addr, ipAddrStr, INET_ADDRSTRLEN);
      char* hostStr = parent->addrToHost(ipAddrStr);
      // Check if a connection already exists with the remote relay
      if (parent->acceptedConnections.count(string(hostStr)) != 0) {
         int serverTcpSdOld = parent->acceptedConnections[string(hostStr)];
         pthread_t* relayOutTOld = parent->relayOutThreads[string(hostStr)];
         // Cancel relayOutT thread
         int relayOutCancelValue = pthread_cancel(*relayOutTOld);
         if (relayOutCancelValue != 0) {
            cerr << "pthread_cancel() failed: acceptsThread" << endl;
            exit(EXIT_FAILURE);
         }
         // Close the socket descriptor
         close(serverTcpSdOld);
         // Clean dynamic memory
         delete relayOutTOld;
         relayOutTOld = NULL;
         // Remove connection from maps
         parent->acceptedConnections.erase(string(hostStr));
         parent->relayOutThreads.erase(string(hostStr));
      }
      if (parent->addedConnections.count(string(hostStr)) != 0) {
         int serverTcpSdOld = parent->addedConnections[string(hostStr)];
         pthread_t* relayOutTOld = parent->relayOutThreads[string(hostStr)];
         // Cancel relayOutT thread
         int relayOutCancelValue = pthread_cancel(*relayOutTOld);
         if (relayOutCancelValue != 0) {
            cerr << "pthread_cancel() failed: acceptsThread" << endl;
            exit(EXIT_FAILURE);
         }
         // Close the socket descriptor
         close(serverTcpSdOld);
         // Clean dynamic memory
         delete relayOutTOld;
         relayOutTOld = NULL;
         // Remove connection from maps
         parent->addedConnections.erase(string(hostStr));
         parent->relayOutThreads.erase(string(hostStr));
      }
      // Create relay out thread
      pthread_t* relayOutT = new pthread_t;
      RelayOutParam* param = new RelayOutParam(parent, hostStr, serverTcpSd);
      int relayOutCreateValue = pthread_create(relayOutT, NULL, relayOutThread, (void*)param);
      if (relayOutCreateValue != 0) {
         cerr << "pthread_create() failed: acceptsThread" << endl;
         exit(EXIT_FAILURE);
      }
      // Add values to maps to keep track of this connection
      parent->acceptedConnections[string(hostStr)] = serverTcpSd;
      parent->relayOutThreads[string(hostStr)] = relayOutT;
      // Print confirmation message
      printf("> accepted %s (%s) on port %d, sd = %d\n", hostStr, ipAddrStr, port, serverTcpSd);
   }
   // exit this thread
   pthread_exit(EXIT_SUCCESS);
}

/*******************************************************************************
 * Command Thread
 * Waits for a user to type the following commands to standard input (i.e. cin):
 * add, delete, show, help, and quit.
 * @param arg           a pointer to the this thread's parent, the UdpRelay object
 */
void* UdpRelay::commandThread(void* arg) {
   UdpRelay* parent = (UdpRelay*)arg;
   while (parent->udpRelayRunning) {
      printf("# ");
      // Get user input and parse command
      char userInput[COMMAND_MAX_SIZE];
      memset(userInput, 0, COMMAND_MAX_SIZE);
      cin.getline(userInput, COMMAND_MAX_SIZE);
      if (strlen(userInput) == 0) {
         continue;
      }
      if (!cin.good()) {
         parent->commandInvalid(userInput);
         cin.clear();
         cin.ignore(INT_MAX, '\n');
         continue;
      }
      char* command = strtok(userInput, " \0");
      // DECIDE WHICH COMMAND TO EXECUTE
      if (strcmp(command, "add") == 0) {
         char* remoteIp = strtok(NULL, ":");
         if (remoteIp == NULL) {
            printf("invalid remoteIp: %s\n", remoteIp);
            continue;
         }
         char* remotePortStr = strtok(NULL, "\0");
         if (remotePortStr == NULL) {
            printf("invalid remotePort: %s\n", remotePortStr);
            continue;
         }
         int remotePort = atoi(remotePortStr);
         if (remotePort < 0 || 65536 < remotePort) {
            printf("port must be > 0 and < 65536\n");
            continue;
         }
         parent->commandAdd(remoteIp, remotePort);
      } else if (strcmp(command, "delete") == 0) {
         char* remoteIp = strtok(NULL, "\0");
         if (remoteIp == NULL) {
            printf("invalid remoteIp: %s\n", remoteIp);
            continue;
         }
         parent->commandDelete(remoteIp);
      } else if (strcmp(command, "show") == 0) {
         parent->commandShow();
      } else if (strcmp(command, "help") == 0) {
         parent->commandHelp();
      } else if (strcmp(command, "quit") == 0) {
         parent->commandQuit();
      } else {
         parent->commandInvalid(userInput);
      }
   }
   // exit this thread
   pthread_exit(EXIT_SUCCESS);
}

/*******************************************************************************
 * Relay In Thread
 * Catches UDP multicast messages over a local network group and processes them.
 * Determines if the local group has already seen the message by examining the
 * header of the message. If so, the message is ignored. If not, the group's ip
 * address is added to the header so it will be ignored next time, the message
 * is sent to remote networks connected to this network.
 * @param arg           a pointer to the this thread's parent, the UdpRelay object
 */
void* UdpRelay::relayInThread(void* arg) {
   // Extract parameters
   UdpRelay* parent = (UdpRelay*)arg;
   // Initialize variables
   int serverSd = parent->localUdpMulticast->getServerSocket();
   if (serverSd == -1) {
      cerr << "UdpMulticast.getServerSocket() failed: relayInThread" << endl;
      exit(EXIT_FAILURE);
   }
   // Loop to catch local UDP multicast messages
   while (true) {
      char* localMessage = new char[BUFFER_MAX_SIZE];
      // Receive a message
      if (!parent->localUdpMulticast->recv(localMessage, BUFFER_MAX_SIZE)) {
         cerr << "UdpMulticast.recv() failed: relayInThread" << endl;
         exit(EXIT_FAILURE);
      }
      // Lock mutex on msgBuffer
      if (pthread_mutex_lock(parent->msgMutex) != 0) {
         cerr << "pthread_mutex_lock() failed: relayInThread" << endl;
         exit(EXIT_FAILURE);
      }
      parent->msgBuffer = localMessage;
      localMessage = NULL;
      // Determine if message header contains groupIP
      bool headerContainsGroupIp = parent->checkMsgForGrpIp();
      if (headerContainsGroupIp) {
         continue;
      } else {
         parent->addGrpIpToMsgHead();
         parent->sendMsgToRemoteGrp();
      }
      // Signal realyOutThread
      if (pthread_cond_signal(parent->cond) != 0) {
         cerr << "pthread_cond_signal() failed: relayInThread" << endl;
         exit(EXIT_FAILURE);
      }
      // Unlock mutex on msgBuffer
      if (pthread_mutex_unlock(parent->msgMutex) != 0) {
         cerr << "pthread_mutex_unlock() failed: relayInThread" << endl;
         exit(EXIT_FAILURE);
      }
   }
   // exit this thread
   pthread_exit(EXIT_SUCCESS);
}

/*******************************************************************************
 * Relay Out Thread
 * Catches TCP messages from a remote network and examines the header of the
 * message to determine if the local network has already seen it. If so, the
 * message is ignored. If not, the message is broadcast over UDP multicast to
 * the local network.
 * @param arg           a pointer to a RelayOutThreadParam object
 */
void* UdpRelay::relayOutThread(void* arg) {
   // Extract parameters
   RelayOutParam* param = (RelayOutParam*)arg;
   UdpRelay* parent = param->self;
   char* peerAddr = param->addr;
   int tcpSd = param->sd;
   // Clean up dynamic memory that is not needed anymore
   delete param;
   param = NULL;
   // Loop to catch remote TCP messages
   while (true) {
      char* localMessage = new char[BUFFER_MAX_SIZE];
      // Receive a message
      int recvValue = recv(tcpSd, localMessage, BUFFER_MAX_SIZE, 0);
      if (recvValue < 0) {
         cerr << "recv() failed: relayOutThread" << endl;
         pthread_exit((void*)EXIT_FAILURE);
      }
      if (recvValue == 0) {
         // The peer has closed the connection
         break;
      }
      // Lock mutex on msgBuffer
      if (pthread_mutex_lock(parent->msgMutex) != 0) {
         cerr << "pthread_mutex_lock() failed: relayOutThread" << endl;
         pthread_exit((void*)EXIT_FAILURE);
      }
      parent->msgBuffer = localMessage;
      localMessage = NULL;
      // Determine if message header contains groupIP
      bool headerContainsGroupIp = parent->checkMsgForGrpIp();
      if (headerContainsGroupIp) {
         continue;
      } else {
         // Get the UDP socket descriptor for multicast
         int clientSd = parent->localUdpMulticast->getClientSocket();
         if (clientSd == -1) {
            cerr << "UdpMulticast.getClientSocket() failed: relayOutThread" << endl;
            pthread_exit((void*)EXIT_FAILURE);
         }
         parent->printMsgBuffer();
         // Multicast the message locally over UDP
         if (!parent->localUdpMulticast->multicast(parent->msgBuffer)) {
            cerr << "UdpMulticast.multicast() failed: relayOutThread" << endl;
            pthread_exit((void*)EXIT_FAILURE);
         }
         // Wait for relayInThread to process message
         if (pthread_cond_wait(parent->cond, parent->msgMutex) != 0) {
            cerr << "pthread_cond_wait() failed: relayOutThread" << endl;
            pthread_exit((void*)EXIT_FAILURE);
         }
         printf("> broadcast %d bytes to %s:%d\n", (int)strlen(parent->msgBuffer), parent->groupIp, parent->groupPort);
      }
      // Unlock mutex on msgBuffer
      if (pthread_mutex_unlock(parent->msgMutex) != 0) {
         cerr << "pthread_mutex_unlock() failed: relayOutThread" << endl;
         pthread_exit((void*)EXIT_FAILURE);
      }
   }
   // remove this thread from the maps
   if (parent->acceptedConnections.count(string(peerAddr)) != 0) {
      parent->acceptedConnections.erase(string(peerAddr));
   }
   if (parent->addedConnections.count(string(peerAddr)) != 0) {
      parent->addedConnections.erase(string(peerAddr));
   }
   if (parent->relayOutThreads.count(string(peerAddr)) != 0) {
      parent->relayOutThreads.erase(string(peerAddr));
      pthread_t* relayOutT = parent->relayOutThreads[string(peerAddr)];
      // Clean dynamic memory
      delete relayOutT;
      relayOutT = NULL;
   }
   // exit this thread
   pthread_exit(EXIT_SUCCESS);
}

/*******************************************************************************
 * Add Command
 * A method to open a TCP connection with a remote network segment or group. If
 * a remote connection with the given address already existing, the old
 * connection is first removed before the new one is opened.
 * @param remoteIp      a c-string representing the ip address of the connection
 * @param remotePort    an integer representing the port of the connection
 */
void UdpRelay::commandAdd(char* remoteIp, int remotePort) {
   // Check if a connection already exists with the remote relay
   if (acceptedConnections.count(string(remoteIp)) != 0) {
      int serverTcpSdOld = acceptedConnections[string(remoteIp)];
      pthread_t* relayOutTOld = relayOutThreads[string(remoteIp)];
      // Cancel relayOutT thread
      int relayOutCancelValue = pthread_cancel(*relayOutTOld);
      if (relayOutCancelValue != 0) {
         cerr << "pthread_cancel() failed: commandAdd" << endl;
         exit(EXIT_FAILURE);
      }
      // Close the socket descriptor
      close(serverTcpSdOld);
      // Clean dynamic memory
      delete relayOutTOld;
      relayOutTOld = NULL;
      // Remove connection from maps
      acceptedConnections.erase(string(remoteIp));
      relayOutThreads.erase(string(remoteIp));
   }
   if (addedConnections.count(string(remoteIp)) != 0) {
      int serverTcpSdOld = addedConnections[string(remoteIp)];
      pthread_t* relayOutTOld = relayOutThreads[string(remoteIp)];
      // Cancel relayOutT thread
      int relayOutCancelValue = pthread_cancel(*relayOutTOld);
      if (relayOutCancelValue != 0) {
         cerr << "pthread_cancel() failed: commandAdd" << endl;
         exit(EXIT_FAILURE);
      }
      // Close the socket descriptor
      close(serverTcpSdOld);
      // Clean dynamic memory
      delete relayOutTOld;
      relayOutTOld = NULL;
      // Remove connection from maps
      addedConnections.erase(string(remoteIp));
      relayOutThreads.erase(string(remoteIp));
   }
   // Add a new TCP connection
   int clientTcpSd = localSocket->getClientSocket(remoteIp);
   // Retrieve hostname and ipaddress (NOTE: this code is duplicated in acceptsThread)
   struct sockaddr_storage sockAddrStorage;
   memset(&sockAddrStorage, 0, sizeof(sockAddrStorage));
   struct sockaddr_in sockAddrIn;
   memset(&sockAddrIn, 0, sizeof(sockAddrIn));
   char ipAddrStr[INET_ADDRSTRLEN];
   socklen_t len = sizeof(sockAddrStorage);
   getpeername(clientTcpSd, (struct sockaddr*)&sockAddrStorage, &len);
   struct sockaddr_in* sockAddrInPtr = (struct sockaddr_in*)&sockAddrStorage;
   unsigned short int port = ntohs(sockAddrInPtr->sin_port);
   inet_ntop(AF_INET, &sockAddrInPtr->sin_addr, ipAddrStr, INET_ADDRSTRLEN);
   char* hostStr = addrToHost(ipAddrStr);
   // Create relay out thread
   pthread_t* relayOutT = new pthread_t;
   RelayOutParam* param = new RelayOutParam(this, remoteIp, clientTcpSd);
   int relayOutCreateValue = pthread_create(relayOutT, NULL, relayOutThread, (void*)param);
   if (relayOutCreateValue != 0) {
      cerr << "pthread_create() failed: commandAdd" << endl;
      exit(EXIT_FAILURE);
   }
   // Add values to maps to keep track of this connection
   addedConnections[string(remoteIp)] = clientTcpSd;
   relayOutThreads[string(remoteIp)] = relayOutT;
   // Print confirmation message
   printf("> added %s, sd = %d\n", hostStr, clientTcpSd);
}

/*******************************************************************************
 * Delete Command
 * A method to close a TCP connection with a remote network segment or group.
 * The the remoteIp is valid, this routine cancels the relayOut thread
 * associated with the remote connection and closes the socketDescriptor
 * associated with the remote connection.
 * @param remoteIp      a c-string representing the ip address of the connection
 */
void UdpRelay::commandDelete(char* remoteIp) {
   string remoteIpStr = string(remoteIp);
   // Lookup relayOut thread corresponding to the remoteIp
   if (addedConnections.count(remoteIpStr) == 0 ||
       relayOutThreads.count(remoteIpStr) == 0) {
      printf("> connection does not exist: %s\n", remoteIp);
      return;
   }
   int tcpSd = addedConnections[remoteIpStr];
   pthread_t* relayOutT = relayOutThreads[remoteIpStr];
   // Cancel relayOutT thread
   int relayOutCancelValue = pthread_cancel(*relayOutT);
   if (relayOutCancelValue != 0) {
      cerr << "pthread_cancel() failed: commandDelete" << endl;
      exit(EXIT_FAILURE);
   }
   // Close the socket descriptor
   close(tcpSd);
   // Clean dynamic memory
   delete relayOutT;
   relayOutT = NULL;
   // Remove connection from maps
   addedConnections.erase(remoteIpStr);
   relayOutThreads.erase(remoteIpStr);
   // Print confirmation message
   printf("> deleted %s, sd = %d\n", remoteIp, tcpSd);
}

/*******************************************************************************
 * Help Command
 * A method to print instructions for how to use the command interface.
 */
void UdpRelay::commandHelp() {
   printf("UdpRelay.commandThread: accepts...");   printf("\n");
   printf("   add remoteIp:remoteTcpPort");        printf("\n");
   printf("   delete remoteIp");                   printf("\n");
   printf("   show");                              printf("\n");
   printf("   help");                              printf("\n");
   printf("   quit");                              printf("\n");
}

/*******************************************************************************
 * Invalid Command
 * A method to print text for when an invalid command is received.
 */
void UdpRelay::commandInvalid(char* command) {
   printf("%s is not a recognizable command.\n", command);
   printf("Please use 'help' for more information.\n");
}

/*******************************************************************************
 * Quit Command
 * A method to signal the UdpRelay to close connections, exit running threads,
 * and proceed to the end of the process.
 */
void UdpRelay::commandQuit() {
   // udpRelayRunning is used by commandT
   udpRelayRunning = false;
   // Cancel accepts thread
   int acceptsCancelValue = pthread_cancel(UdpRelay::acceptsT);
   if (acceptsCancelValue != 0) {
      cerr << "pthread_cancel() failed: acceptsThread" << endl;
      exit(EXIT_FAILURE);
   }
   // Cancel relay in thread
   int relayInCancelValue = pthread_cancel(UdpRelay::relayInT);
   if (relayInCancelValue != 0) {
      cerr << "pthread_cancel() failed: relayInThread" << endl;
      exit(EXIT_FAILURE);
   }
   // Loop through the relayOutThreads connection map and cancel each thread
   for(map<string,pthread_t*>::iterator relayOutThreadItr = relayOutThreads.begin();
       relayOutThreadItr != relayOutThreads.end();
       relayOutThreadItr++) {
      string addrStr = relayOutThreadItr->first;
      pthread_t* relayOutT = relayOutThreadItr->second;
      // Cancel relayOutT thread
      int relayOutCancelValue = pthread_cancel(*relayOutT);
      if (relayOutCancelValue != 0) {
         cerr << "pthread_cancel() failed: commandQuit" << endl;
         exit(EXIT_FAILURE);
      }
      // Clean dynamic memory
      delete relayOutT;
      relayOutT = NULL;
      // Remove connection from map
      relayOutThreads.erase(addrStr);
   }
   // Loop through the accepted connection map and stop each connection
   for(map<string,int>::iterator remoteConnectionItr = acceptedConnections.begin();
       remoteConnectionItr != acceptedConnections.end();
       remoteConnectionItr++) {
      string addrStr = remoteConnectionItr->first;
      int serverTcpSd = remoteConnectionItr->second;
      // Close the socket descriptor
      close(serverTcpSd);
      // Remove connection from map
      acceptedConnections.erase(addrStr);
   }
   // Loop through the added connection map and stop each connection
   for(map<string,int>::iterator remoteConnectionItr = addedConnections.begin();
       remoteConnectionItr != addedConnections.end();
       remoteConnectionItr++) {
      string addrStr = remoteConnectionItr->first;
      int serverTcpSd = remoteConnectionItr->second;
      // Close the socket descriptor
      close(serverTcpSd);
      // Remove connection from map
      acceptedConnections.erase(addrStr);
   }
}

/*******************************************************************************
 * Show Command
 * A method to print all TCP connections to remote network segments or groups.
 * The format of the output is "hostname : filedescriptor" for each line.
 */
void UdpRelay::commandShow() {
   // Loop through the remote connection map and print each key
   for(map<string,int>::iterator remoteConnectionItr = addedConnections.begin();
       remoteConnectionItr != addedConnections.end();
       remoteConnectionItr++) {
      printf("%s : %d\n", (remoteConnectionItr->first).c_str(), remoteConnectionItr->second);
   }
}

/*******************************************************************************
 * Add the Group IP to the Message Header
 * A private helper function to ...
 */
void UdpRelay::addGrpIpToMsgHead() {
   char* originalMessage = msgBuffer;
   int lengthOfOriginalMessage = strlen(originalMessage);
   int lengthOfModifiedMessage = lengthOfOriginalMessage + 4;
   char modifiedMessage[BUFFER_MAX_SIZE];
   memset(modifiedMessage, 0, BUFFER_MAX_SIZE);
   int lengthOfOriginalHeader = 4*(originalMessage[3]+1);  // i.e. startOfMessage
   int lengthOfMessageText = lengthOfOriginalMessage - lengthOfOriginalHeader;
   int lengthOfModifiedHeader = lengthOfOriginalHeader + 4;
   // Copy first 3 constant bytes
   memcpy(modifiedMessage, originalMessage, 3);
   // Assign hop count with incremented value
   modifiedMessage[3] = originalMessage[3] + 1;
   // Copy the remainder of the original header
   memcpy(modifiedMessage+4, originalMessage+4, lengthOfOriginalHeader-4);
   // Insert the groupIp into the end of the modified header
   char* bytes = hostaddrToBytes(groupIp);
   memcpy(modifiedMessage+lengthOfOriginalHeader, bytes, RAW_ADDR_SIZE);
   delete bytes;
   bytes = NULL;
   // Copy the message
   memcpy(modifiedMessage+lengthOfModifiedHeader, originalMessage+lengthOfOriginalHeader, lengthOfMessageText);
   // Update the pointer to the message
   memcpy(msgBuffer, modifiedMessage, BUFFER_MAX_SIZE);
}

/*******************************************************************************
 * Use gethostbyaddr() to convert ip address to hostname
 * @param addr          a c-string representing the ip address (e.g. "127.0.0.1")
 * @return              a c-string representing the hostname
 */
char* UdpRelay::addrToHost(const char* addr) {
   struct in_addr inAddr;
   memset(&inAddr, 0, sizeof(inAddr));
   if (inet_pton(AF_INET, addr, &inAddr) != 1) {
      cerr << "inet_pton() failed: addrToHost" << endl;
      exit(EXIT_FAILURE);
   }
   struct hostent* he;
   he = gethostbyaddr(&inAddr, sizeof(inAddr), AF_INET);
   return he->h_name;
}

/*******************************************************************************
 * Use inet_ntop() to convert bytes to ip address.
 * @param bytes         a c-string representing the bytes
 * @return              a c-string representing the ip address
 */
void UdpRelay::bytesToAddr(const char* bytes, char* addr) {
   char ipAddrStr[INET_ADDRSTRLEN];
   struct in_addr inAddr;
   memset(&ipAddrStr, 0, INET_ADDRSTRLEN);
   memset(&inAddr, 0, sizeof(inAddr));
   memcpy(&inAddr.s_addr, bytes, INET_ADDRSTRLEN);
   if (inet_ntop(AF_INET, &inAddr, ipAddrStr, INET_ADDRSTRLEN) == NULL) {
      cerr << "inet_ntop() failed: bytesToAddr" << endl;
      exit(EXIT_FAILURE);
   }
   memcpy(addr, ipAddrStr, INET_ADDRSTRLEN);
}

/*******************************************************************************
 * Check Message for Group Ip
 * A private helper function to ...
 */
bool UdpRelay::checkMsgForGrpIp() {
   char* message = msgBuffer;
   int hopCt = (int)message[3];
   // Loop through the message header to find a match with the groupIp
   for (int i = 0; i < hopCt; i++) {
      char* ipAddrStr = new char[INET_ADDRSTRLEN];
      bytesToAddr(message+(4*(i+1)), ipAddrStr);
      if (strcmp(ipAddrStr, groupIp) == 0) {
         return true;
      }
      delete ipAddrStr;
      ipAddrStr = NULL;
   }
   // The groupIp was not found in the message header
   return false;
}

/*******************************************************************************
 * Create Threads
 * A private helper function to create the persistent threads.
 */
void UdpRelay::createThreads() {
   // Create accepts thread
   int acceptsCreateValue = pthread_create(&acceptsT, NULL, acceptsThread, (void*)this);
   if (acceptsCreateValue != 0) {
      cerr << "pthread_create() failed: accepts" << endl;
      exit(EXIT_FAILURE);
   }
   // Create command thread
   int commandCreateValue = pthread_create(&commandT, NULL, commandThread, (void*)this);
   if (commandCreateValue != 0) {
      cerr << "pthread_create() failed: command" << endl;
      exit(EXIT_FAILURE);
   }
   // Create relay in thread
   int relayInCreateValue = pthread_create(&relayInT, NULL, relayInThread, (void*)this);
   if (relayInCreateValue != 0) {
      cerr << "pthread_create() failed: relayIn" << endl;
      exit(EXIT_FAILURE);
   }
}

/*******************************************************************************
 * Use inet_pton() to convert hostname/ip address to bytes
 * @param message       a c-string representing the hostname/ip address
 * @return              a c-string representing the bytes
 */
char* UdpRelay::hostaddrToBytes(const char* hostaddr) {
   struct in_addr inAddr;
   memset(&inAddr, 0, sizeof(inAddr));
   if (inet_pton(AF_INET, hostaddr, &inAddr) != 1) {
      cerr << "inet_pton() failed: hostaddrToBytes" << endl;
      exit(EXIT_FAILURE);
   }
   char* bytes = new char[RAW_ADDR_SIZE];
   memcpy(bytes, &inAddr.s_addr, RAW_ADDR_SIZE);
   return bytes;
}

/*******************************************************************************
 * Join Threads
 * A private helper function to join the persistent threads.
 */
void UdpRelay::joinThreads() {
   // Join accepts thread
   int acceptsJoinValue = pthread_join(acceptsT, NULL);
   if (acceptsJoinValue != 0) {
      cerr << "pthread_join() failed: accepts" << endl;
      exit(EXIT_FAILURE);
   }
   // Join command thread
   int commandJoinValue = pthread_join(commandT, NULL);
   if (commandJoinValue != 0) {
      cerr << "pthread_join() failed: command" << endl;
      exit(EXIT_FAILURE);
   }
   // Join relay in thread
   int relayInJoinValue = pthread_join(relayInT, NULL);
   if (relayInJoinValue != 0) {
      cerr << "pthread_join() failed: relayIn" << endl;
      exit(EXIT_FAILURE);
   }
}

/*******************************************************************************
 * Send Message to Remote Groups
 * A private helper function to send a message to all of the open remote
 * connections.
 * @param message       a c-string representing the message to be broadcast
 */
void UdpRelay::sendMsgToRemoteGrp() {
   // Loop through the remote connection map and broadcast message to each one
   for(map<string,int>::iterator connectionItr = addedConnections.begin();
       connectionItr != addedConnections.end();
       connectionItr++) {
      int tcpSd = connectionItr->second;
      int lengthOfMessage = strlen(msgBuffer);
      // Send the message
      if (send(tcpSd, msgBuffer, lengthOfMessage, 0) < 0) {
         cerr << "send() failed: sendMsgToRemoteGrp" << endl;
         exit(EXIT_FAILURE);
      }
      printf("> relay %d bytes to remoteGroup[%s]\n", lengthOfMessage, (connectionItr->first).c_str());
   }
}

/*******************************************************************************
 * Print Message Received
 * A private helper function to print the received message.
 * @param message       a c-string representing the message
 */
void UdpRelay::printMsgBuffer() {
   char* message = msgBuffer;
   // Determine which host sent the message;
   char* ipAddrStr = new char[INET_ADDRSTRLEN];
   bytesToAddr(message+4, ipAddrStr);
   char* hostStr = addrToHost(ipAddrStr);
   // Determine the message properties
   int lengthOfMessage = strlen(message);
   int lengthOfHeader = 4*(message[3]+1);  // i.e. startOfMessage
   char* msgText = message+lengthOfHeader;
   printf("> received %d bytes from %s: %s\n", lengthOfMessage, hostStr, msgText);
   // Clean up dynamic memory within the scope of this method
   delete ipAddrStr;
   ipAddrStr = NULL;
}
