// Library
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <calcLib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

// Headers
#include "protocol.h"

using namespace std;

/* Needs to be global, to be reachable by callback and main */

// Global Variables
int loopCount = 0;
int terminate = 0;
int noClients = 0;

/* Call back function, will be called when the SIGALRM is raised when the timer expires. */
// As anybody can call the handler, its good coding to check the signal number that called it.

struct Client 
{
  struct sockaddr_in IP;
  struct calcProtocol protoAns;
};


void checkJobList(int signum)
{
  printf("Let me be, I want to sleep.\n");

  if(loopCount > 20)
  {
    printf("I had enough.\n");
    terminate = 1;
  }

  /*
  if (noClients > 0)
  {
    noClients--;
  }
  */

  return;
}

int main(int argc, char *argv[])
{

  // Validate Program Call
  if(argc < 2)
  {
    fprintf(stderr, "Not enough arguments in program call.\n");
    exit(1);
  }
  if(strlen(argv[1]) < 12)
  {
    fprintf(stderr, "No port found in program call.\n");
    exit(1);
  }

  char delim[] = ":";
  char* serverIP = strtok(argv[1], delim);
  char* serverPort = strtok(NULL, delim);
  int serverPortInt = atoi(serverPort);

  printf("Server IP: %s | Server Port: %s\n", serverIP, serverPort);

  // Declare Variables
  
  int rv;
  int reUse = 1;
  int sockFD;
  int sockTries = 1;
  
  struct addrinfo prototype;
  struct addrinfo* server;
  struct addrinfo* ptr;

  memset(&prototype, 0, sizeof(prototype));
  prototype.ai_family = AF_INET;
  prototype.ai_socktype = SOCK_DGRAM;
  prototype.ai_flags = AI_PASSIVE;

  rv = getaddrinfo(serverIP, serverPort, &prototype, &server);

  if(rv != 0)
  {
    fprintf(stderr, "Error: getaddrinfo: %s\n", gai_strerror(rv));
    exit(1);
  }

  for (ptr = server; ptr != NULL; ptr = ptr->ai_next)
  {
    sockFD = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (sockFD == -1)
    {
      printf("Error creating socket %d time(s).\n", sockTries);
      sockTries++;
      continue;
    }

    rv = setsockopt(sockFD, SOL_SOCKET, SO_REUSEADDR, &reUse, sizeof(reUse));
    if (rv == -1)
    {
      perror("Error: REUSE SO");
    }
  }

  freeaddrinfo(server);

  struct sockaddr_in serverAddr;
  memset(&serverAddr, 0, sizeof(serverAddr));
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(serverPortInt);
  inet_aton(serverIP, &serverAddr.sin_addr);

  rv = bind(sockFD, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
  if (rv != 0)
  {
    perror("Error: Bind");
    exit(1);
  }

  printf("Listening to %s:%s\n\n", inet_ntoa(serverAddr.sin_addr), serverPort);

  struct itimerval alarmTime;
  alarmTime.it_interval.tv_sec = 10;
  alarmTime.it_interval.tv_usec = 0;
  alarmTime.it_value.tv_sec = 10;
  alarmTime.it_value.tv_usec = 0;

  signal(SIGALRM, checkJobList);
  setitimer(ITIMER_REAL, &alarmTime, NULL); // Start/register the alarm. 

  struct sockaddr_in clientAddr;
  memset(&clientAddr, 0, sizeof(clientAddr));
  socklen_t clientLen = sizeof(clientAddr);

  struct calcMessage mBuffer;
  struct calcProtocol pBuffer;
  
  uint32_t id = 1;
  struct Client clientList[30];
  
  struct Client nc;
  initCalcLib();

  
  // Main Loop
  while (terminate == 0)
  {
    printf("Number of Loops: %d times.\n",loopCount);
    sleep(1);
    loopCount++;

    printf("Waiting for incoming clients...\n");

    // Redirect client depending on message
    rv = recvfrom(sockFD, &mBuffer, sizeof(pBuffer), 0, (struct sockaddr*)&clientAddr, &clientLen);
    printf("Recieved %d bytes\n", rv);

    // recvfrom error
    if (rv < 0)
    {
      perror("Error: Recvfrom (1)");
      continue;
    }

    // client sent 0 bytes
    else if (rv == 0)
    {
      printf("Recieved 0 bytes\n");
      continue;
    }

    // client sent calc msg
    else if (rv == sizeof(calcMessage))
    {

      printf("Type: %d | Message: %d | Protocol: %d | Major: %d | Minor: %d\n",
      ntohs(mBuffer.type), ntohl(mBuffer.message), ntohs(mBuffer.protocol), ntohs(mBuffer.major_version), ntohs(mBuffer.minor_version));
      
      // Client sends succesfully
      if (ntohs(mBuffer.type) == 22 && ntohl(mBuffer.message) == 0 && ntohs(mBuffer.protocol) == 17 && ntohs(mBuffer.major_version) == 1 && ntohs(mBuffer.minor_version) == 0)
      {
        printf("Recieved calcMessage from ");
        printf("IP and Port | %s:%d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

        // Prepare protocol message
        nc.protoAns.id = htonl(id++);
        nc.protoAns.type = htons(1);
        nc.protoAns.major_version = htons(1);
        nc.protoAns.minor_version = htons(0);
        
        // Create Assignment
        char* typeHolder = randomType();

        // Float Assignment
        if (typeHolder[0] == 'f')
        {
          nc.protoAns.inValue1 = htonl(0);
          nc.protoAns.inValue2 = htonl(0);
          
          nc.protoAns.flValue1 = randomFloat();
          nc.protoAns.flValue2 = randomFloat();

          if (strcmp(typeHolder, "fadd") == 0)
          {
            nc.protoAns.flResult = nc.protoAns.flValue1 + nc.protoAns.flValue2;
            nc.protoAns.arith = htonl(5);
          }
          else if (strcmp(typeHolder, "fsub") == 0)
          {
            nc.protoAns.flResult = nc.protoAns.flValue1 - nc.protoAns.flValue2;
            nc.protoAns.arith = htonl(6);
          }
          else if (strcmp(typeHolder, "fmul") == 0)
          {
            nc.protoAns.flResult = nc.protoAns.flValue1 * nc.protoAns.flValue2;
            nc.protoAns.arith = htonl(7);
          }
          else if (strcmp(typeHolder, "fdiv") == 0)
          {
            nc.protoAns.flResult = nc.protoAns.flValue1 / nc.protoAns.flValue2;
            nc.protoAns.arith = htonl(8);
          } 
          else
          {
            printf("No Float typeholder found.\n");
            continue;
          }
        }
        else
        {
          // Int Assignment
          nc.protoAns.flValue1 = htonl(0);
          nc.protoAns.flValue2 = htonl(0);
          
          nc.protoAns.inValue1 = htonl(randomInt());
          nc.protoAns.inValue2 = htonl(randomInt());

          if (strcmp(typeHolder, "add") == 0)
          {
            nc.protoAns.inResult = htonl(nc.protoAns.inValue1 + nc.protoAns.inValue2);
            nc.protoAns.arith = htonl(1);
          }
          else if (strcmp(typeHolder, "sub") == 0)
          {
            nc.protoAns.inResult = htonl(nc.protoAns.inValue1 - nc.protoAns.inValue2);
            nc.protoAns.arith = htonl(2);
          }
          else if (strcmp(typeHolder, "mul") == 0)
          {
            nc.protoAns.inResult = htonl(nc.protoAns.inValue1 * nc.protoAns.inValue2);
            nc.protoAns.arith = htonl(3);
          }
          else if (strcmp(typeHolder, "div") == 0)
          {
            nc.protoAns.inResult = htonl(nc.protoAns.inValue1 / nc.protoAns.inValue2);
            nc.protoAns.arith = htonl(4);
          }
          else
          {
            printf("No Int typeholder found.\n");
            continue;
          }
        }  

        // Set Answer to 0 before sending
        nc.protoAns.flResult = htonl(0);
        nc.protoAns.inResult = htonl(0);

        printf("Float 1: %8.8g\t Float 2: %8.8g\n", nc.protoAns.flValue1, nc.protoAns.flValue2);
        printf("Int 1: %d\t Int 2: %d\n", nc.protoAns.inValue1, nc.protoAns.inValue2);

        // Send assignment to client
        rv = sendto(sockFD, &nc.protoAns, sizeof(nc.protoAns), 0, (struct sockaddr*)&clientAddr, clientLen);
        if (rv < 1)
        {
          perror("send assignment");
          continue;
        }
        else
        {
          printf("Sent %d bytes to %s:%d\n", rv,  inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
          clientList[noClients++] = nc;

          // set client timer to 10sec
        }
      }
      // If Lost client a message
      else
      {
        struct calcMessage errorMsg;
        errorMsg.type = htons(2);
        errorMsg.message = htonl(2);
        errorMsg.major_version = htons(1);
        errorMsg.minor_version = htons(0);
        rv = sendto(sockFD, &errorMsg, sizeof(errorMsg), 0, (struct sockaddr*)&clientAddr, clientLen);
        printf("Sent error message to client.\n");
        continue;
      }
    }

    // client sent protocol answer
    else if (rv == sizeof (calcProtocol))
    {
      printf("Recieved calcProtocol from IP and Port | %s:%d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

      for (int i = 0; i < 30; i++)
      {
        if(inet_ntoa(clientAddr.sin_addr) == clientList[i], ntohs(clientAddr.sin_port))
      }

      // Check client ID

      // Check client addr

      // Check client port

      // Assignment

      // Check Clients answer

      // Remove client from clientlist

      // Send error to lost client
      
    }
    

  }

  close(sockFD);
  printf("Server is shutdown.\n");
  return 0;  
}
