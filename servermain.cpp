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

/* Call back function, will be called when the SIGALRM is raised when the timer expires. */
// As anybody can call the handler, its good coding to check the signal number that called it.

void checkJobList(int signum)
{
  printf("Let me be, I want to sleep.\n");

  if(loopCount > 20)
  {
    printf("I had enough.\n");
    terminate = 1;
  }
  
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

  struct addrinfo* clientAddr;
  memset(&clientAddr, 0, sizeof(clientAddr));
  socklen_t clientLen = sizeof(clientAddr);
  char buff[1000];

  struct calcMessage cMsg;
  struct calcProtocol cProto;
  
  // Main Loop
  while (terminate == 0)
  {
    printf("Number of Loops: %d times.\n",loopCount);
    sleep(1);
    loopCount++;

    printf("Waiting for incoming clients...\n");

    // RecvFrom
    rv = recvfrom(sockFD, buff, sizeof(buff), 0, (struct sockaddr*)&clientAddr, &clientLen);
    if (rv < 0)
    {
      perror("Error: Recvfrom (1)");
      continue;
    }
    else if (rv == 0)
    {
      printf("Recieved 0 bytes\n");
      continue;
    }
    
    printf("Recieved %d bytes\n", rv);
    struct calcMessage* temp = (struct calcMessage*)&buff;
    if (temp->message == 0)
    {
      printf("Success!\n");
    }
    // Extract IP:Port, ID and Assignment from Client




    // SendTo


  }

  close(sockFD);
  printf("Server is shutdown.\n");
  return 0;  
}
