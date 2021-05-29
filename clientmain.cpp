// Support library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <calcLib.h>
#include <sys/time.h>
#include <errno.h>

// Header files
#include "protocol.h"

// Defines
#define DEBUG

int main(int argc, char *argv[])
{
 
  int errorIndex;
  int socketFD;
  char* serverIP;
  char* serverPort;
  char delim[] = ":";
  struct addrinfo prep;
  struct addrinfo* server;
  struct addrinfo* pointer;

  // Call Verifications  
  if(argc != 2)
  {
    fprintf(stderr, "Program call was incorrect.\n");
    return 1;
  }

  if(strlen(argv[1]) < 12)
  {
    fprintf(stderr, "No port found.\n");
    return 2;
  }

  // Extract IP Address and port from call
  serverIP = strtok(argv[1], delim);
  serverPort = strtok(NULL, delim);

  if(serverIP == NULL || serverPort == NULL)
  {
    fprintf(stderr, "Invalid server input.\n");
    return 3; 
  }

  // Prepare addrinfo 
  memset(&prep, 0, sizeof(prep));
  prep.ai_family = AF_UNSPEC;
  prep.ai_socktype = SOCK_DGRAM;

  // Fill server with information
  errorIndex = getaddrinfo(serverIP, serverPort, &prep, &server);
  if(errorIndex != 0)
  {
    fprintf(stderr, "Program stopped at getaddrinfo: %s\n", gai_strerror(errorIndex));
    return 4;
  }

  // Traverse the linked list of addrinfo's
  for(pointer = server; pointer != NULL; pointer = pointer->ai_next)
  {

    // Attempt to create socket, if failed, try the next one
    socketFD = socket(pointer->ai_family, pointer->ai_socktype, pointer->ai_protocol);
    if(socketFD == -1)
    {
      continue;
    }
    break;
  }

  // Create a second socket to connect to the server
  int socketFDtemp = socket(pointer->ai_family, pointer->ai_socktype, pointer->ai_protocol);
  errorIndex = connect(socketFDtemp, pointer->ai_addr, pointer->ai_addrlen);
  if(errorIndex == -1)
  {
    fprintf(stderr, "Unable to connect to server.\n");
    return 5;
  }
  
  // Create string buffer big enough for IPV4 and IPV6
  char hostIP[INET6_ADDRSTRLEN];

  if(pointer->ai_family == AF_INET)
  {

    // Get Host Address
    struct sockaddr_in hostAddress;
    socklen_t len = sizeof hostAddress;
    memset(&hostAddress, 0, len);
    getsockname(socketFD, (struct sockaddr*)&hostAddress, &len);
    inet_ntop(AF_INET, &hostAddress.sin_addr, hostIP, len);
    printf("Host Address: %s:%d\n", hostIP, atoi(serverPort));

    //Get Local Address
    char localIP[INET_ADDRSTRLEN];
    struct sockaddr_in localAddress;
    len = sizeof localAddress;
    memset(&localAddress, 0, len);
    getsockname(socketFDtemp, (struct sockaddr*)&localAddress, &len);
    inet_ntop(AF_INET, &localAddress.sin_addr, localIP, sizeof(localIP));
    
    #ifdef DEBUG
    printf("Connected to: %s:%d | Local Address: %s:%d\n",serverIP, atoi(serverPort), localIP, ntohs(localAddress.sin_port));
    #endif
  }
  else if(pointer->ai_family == AF_INET6)
  {

    // Get Host Address IPV6
    struct sockaddr_in6 hostAddress6;
    socklen_t len = sizeof hostAddress6;
    memset(&hostAddress6, 0, len);
    getsockname(socketFD, (struct sockaddr*)&hostAddress6, &len);
    inet_ntop(AF_INET6, &hostAddress6.sin6_addr, hostIP, len);
    printf("Host Address: %s:%d\n", hostIP, atoi(serverPort));


    // Get Local Address IPV6
    char localIP6[INET6_ADDRSTRLEN];
    struct sockaddr_in6 localAddress6;
    len = sizeof localAddress6;
    memset(&localAddress6, 0, len);
    getsockname(socketFDtemp, (struct sockaddr*)&localAddress6, &len);
    inet_ntop(AF_INET6, &localAddress6.sin6_addr, localIP6, sizeof(localIP6));

    #ifdef DEBUG
    printf("Local Address: %s:%d\n", localIP6, ntohs(localAddress6.sin6_port));
    #endif
  }
  else
  {
    fprintf(stderr, "IP Version could not be identified\n");
  }

  freeaddrinfo(server);  
  close(socketFDtemp);

  // Now We can send and recv from the server
  int32_t sentBytes;
  int32_t recvBytes;
  struct calcMessage mBuffer;
  struct calcProtocol pBuffer;

  mBuffer.type = htons(22);
  mBuffer.message = htonl(0);
  mBuffer.protocol = htons(17);
  mBuffer.major_version = htons(1);
  mBuffer.minor_version = htons(0);

  struct timeval timeout;
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;
  
  if(setsockopt(socketFD, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
  {
    fprintf(stderr, "Error at sockopt.\n");
    return 6;
  }
    // First Message to server
    int attemptsLeft = 3;
    while(attemptsLeft > 0)
    {

      sentBytes = sendto(socketFD, &mBuffer, sizeof(mBuffer), 0, pointer->ai_addr, pointer->ai_addrlen);
      if(sentBytes == -1)
      {
        fprintf(stderr, "Unable to send.\n");
        return 7;
      }

      #ifdef DEBUG
      printf("Sent %d bytes.\n", sentBytes);
      #endif

      recvBytes = recvfrom(socketFD, &pBuffer, sizeof(pBuffer), 0, pointer->ai_addr, &pointer->ai_addrlen);
      if(recvBytes == -1)
      {
        printf("Attempts: %d\n", attemptsLeft);
        attemptsLeft--;
        continue;
      }
      else if(recvBytes == sizeof (struct calcMessage))
      {
        fprintf(stderr, "Server responded with calcMessage\n");
        struct calcMessage* temp = (struct calcMessage*)&pBuffer;
        if(ntohl(temp->message) == 2)
        {
          return 9;
        }
      }
      else if(recvBytes == sizeof (struct calcProtocol))
      {
        printf("Recieved calcProt from server.\n");
        break;
      }

    }
    
  #ifdef DEBUG
  printf("Recieved %d bytes\n", recvBytes);
  #endif

  uint16_t pType = ntohs(pBuffer.type);
  uint16_t pMajor = ntohs(pBuffer.major_version);
  uint16_t pMinor = ntohs(pBuffer.minor_version);
  uint32_t pID = ntohl(pBuffer.id);
  uint32_t pArith = ntohl(pBuffer.arith);
  int32_t firstIntValue = ntohl(pBuffer.inValue1);
  int32_t secondIntValue = ntohl(pBuffer.inValue2);
  int32_t intResult = ntohl(pBuffer.inResult);
  double firstFloatValue = pBuffer.flValue1;
  double secondFloatValue = pBuffer.flValue2;
  double floatResult = pBuffer.flResult;


  #ifdef DEBUG
  printf("Type: %d, Major: %d, Minor: %d, ID: %d, Arith: %d\n", pType, pMajor, pMinor, pID, pArith);
  printf("First Int: %d, Second Int: %d\n",  firstIntValue, secondIntValue);
  printf("First Float: %8.8g, Second Float: %8.8g\n", firstFloatValue, secondFloatValue);
  #endif
 
  switch (pArith)
  {
  case 1:
  intResult = firstIntValue + secondIntValue;
  printf("Solve: %d + %d\n",  firstIntValue, secondIntValue);
  printf("Result: %d\n", intResult);  
  break;

  case 2:
  intResult = firstIntValue - secondIntValue;
  printf("Solve: %d - %d\n",  firstIntValue, secondIntValue);  
  printf("Result: %d\n", intResult);
  break;
  
  case 3:
  intResult = firstIntValue * secondIntValue;
  printf("Solve: %d * %d\n",  firstIntValue, secondIntValue);  
  printf("Result: %d\n", intResult);
  break;
  
  case 4:
  intResult = firstIntValue / secondIntValue;
  printf("Solve: %d / %d\n",  firstIntValue, secondIntValue);  
  printf("Result: %d\n", intResult);
  break;
  
  case 5:
  floatResult = firstFloatValue + secondFloatValue;
  printf("Solve: %8.8g + %8.8g\n", firstFloatValue, secondFloatValue);
  printf("Result: %8.8g\n", floatResult);
  break;
  
  case 6:
  floatResult = firstFloatValue - secondFloatValue;
  printf("Solve: %8.8g - %8.8g\n", firstFloatValue, secondFloatValue);
  printf("Result: %8.8g\n", floatResult);
  break;
  
  case 7:
  floatResult = firstFloatValue * secondFloatValue;
  printf("Solve: %8.8g * %8.8g\n", firstFloatValue, secondFloatValue);
  printf("Result: %8.8g\n", floatResult);
  break;
  
  case 8:
  floatResult = firstFloatValue / secondFloatValue;
  printf("Solve: %8.8g / %8.8g\n", firstFloatValue, secondFloatValue);
  printf("Result: %8.8g\n", floatResult);
  break;

  default:
  fprintf(stderr, "Progam stopped at arith.\n");
  break;
  }
  
  if(pArith < 5)
  {
    pBuffer.inResult = htonl(intResult);
  }
  else 
  {
    pBuffer.flResult = floatResult;
  }

  pBuffer.type = htons(2);
  pBuffer.major_version = htons(1);
  pBuffer.minor_version = htons(0);
  pBuffer.id = htonl(pID);

  attemptsLeft = 3;
  while(attemptsLeft > 0)
  {  
    sentBytes = sendto(socketFD, &pBuffer, sizeof(pBuffer), 0, pointer->ai_addr, pointer->ai_addrlen);
    if(sentBytes == -1)
    {
      fprintf(stderr, "Program stopped at send sendcalc.\n");
      return 10;
    }

    #ifdef DEBUG
    printf("Sent %d bytes.\n", sentBytes);
    #endif
    
    recvBytes = recvfrom(socketFD, &mBuffer, sizeof(mBuffer), 0, pointer->ai_addr, &pointer->ai_addrlen);
    if(recvBytes == -1)
    {
      printf("Attempts: %d\n", attemptsLeft);
      attemptsLeft--;
      continue;
    }
    else if(recvBytes == sizeof (calcProtocol))
    {
      fprintf(stderr, "Server responded with calcProt, Terminate\n");
      return 12;
    }
    else if (recvBytes == sizeof (calcMessage))
    {
      break;
    }

  }
  #ifdef DEBUG
  printf("Recieved %d bytes.\n", recvBytes);
  #endif
  
  if(ntohl(mBuffer.message) == 1)
  {
    printf("OK\n");
    
  }
  else if(ntohl(mBuffer.message == 0))
  {
    fprintf(stderr, "Server responded with Not applicable/available\n");
    return 13;
  }
  else
  {
    if(attemptsLeft == 0)
    {
      fprintf(stderr, "Attempts used. Terminate\n");
      return 14;
    }

   fprintf(stderr, "NOT OK.\n");
   return 14;
  }

  
  close(socketFD);

  return 0;
}
