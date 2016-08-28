//
//  main.c
//  AmazonTechTest
//
//  Created by Riccardo Rizzo on 26/08/16.
//  Copyright © 2016 Riccardo Rizzo. All rights reserved.
//

#include <stdio.h>
#include <sys/socket.h>     /* for socket(), bind(), and connect() */
#include <arpa/inet.h>      /* for sockaddr_in and inet_ntoa() */
#include <stdlib.h>         /* for atoi() and exit() */
#include <string.h>         /* for memset() */
#include <unistd.h>         /* for close() */
#include <pthread.h>        /* for threading , link with lpthread */


#define USE_THREAD          /* Comment here if you won't use threaded application */

#define true 1
#define false 0
#define MAX_D 256           /* Size of stack used for calculations */
#define MAXPENDING 5        /* Maximum outstanding connection requests */
#define RCVBUFSIZE 1024     /* Size of receive buffer */
#define TXBUFSIZE 1024      /* Size of receive buffer */

#define TCP_PORT 1234

int rpn_error;
double stack[MAX_D];
int depth;
int sock;
int *new_sock;

//****************
//fn: replyToClient
//return: long
//params: char*, int
//This fn send a string to a connected client and return the buffer sent.
//****************
long replyToClient(char *string, int socket)
{
    char buffer[TXBUFSIZE];        /* Buffer for receving string */
    long toSendLen;                  /* Length of string to send */
    
    /* Send result */
    printf("Sending: %s\n", string);
    sprintf(buffer, "%s", string);
    toSendLen = strlen(string);
    if (send(socket, buffer, toSendLen, 0) != strlen(string))
        return toSendLen;
    return 0;
}

//****************
//fn: concat
//return: char*
//params: char*, char*
//This fn concatenate 2 string and return the concat string.
//****************
char* concat(char *s1, char *s2)
{
    char *result = malloc(strlen(s1)+strlen(s2)+1);//+1 for the zero-terminator
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

//****************
//fn: DieWithError
//return: void
//params: char *, int
//This fn send a string to a connected client the error message and set the error flag to true.
//****************
void DieWithError(char *errorMessage, int sock)
{
    char *errStr = concat("ERROR: ", errorMessage);
    perror(errStr);
    replyToClient(errStr, sock);
    rpn_error = true;
}

//****************
//fn: push and pop
//return: void or double
//params: double or void
//This fns is used with the stack to push and pop values.
//****************
void push(double v)
{
    if (depth >= MAX_D) DieWithError("Stack overflow\n", sock);
    stack[depth++] = v;
}

double pop()
{
    if (!depth) DieWithError("Stack underflow\n",sock);
    return stack[--depth];
}


//****************
//fn: rpn
//return: double
//params: char *, int
//This fn perform the computation of the string passed as parameters. Result the result.
//****************
double rpn(char *s, int sock)
{
    double a, b;
    int i;
    char *e, *w = " \t\n\r\f";
    
    for (s = strtok(s, w); s; s = strtok(0, w)) {
        a = strtod(s, &e);
        if (e > s)		printf(" :"), push(a);
#define binop(x) printf("%c:", *s), b = pop(), a = pop(), push(x)
        else if (*s == '+')	binop(a + b);
        else if (*s == '-')	binop(a - b);
        else if (*s == '*')	binop(a * b);
        else if (*s == '/')	binop(a / b);
#undef binop
        else {
            fprintf(stderr, "'%c': ", *s);
            DieWithError("Unknown operator\n", sock);
            break;
        }
        for (i = depth; i-- || 0 * putchar('\n'); )
            printf(" %g", stack[i]);
    }
    
    if (depth != 1) DieWithError("Stack Leftover\n",sock);
    
    return pop();
}

//****************
//fn: ExitWithError
//return: void
//params: char *
//This fn close the program with an error message
//****************
void ExitWithError(char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}

//****************
//fn: CloseSock
//return: void
//params: int
//This fn close the socket connection passes as parameter.
//****************
void CloseSock(int *sock)
{
    if(*sock != (int)NULL) {
        close(*sock);
        *sock = (int)NULL;
    }
}


//****************
//fn: HandleTCPClient
//return: void
//params: void *
//This fn manage an accepted TCP connection.
//****************
void *HandleTCPClient(void * sock)
{
    char buffer[RCVBUFSIZE];        /* Buffer for receving string */
    long bytesRcvd;                  /* Number of bytes received */
    char *rcvd;                     /* String received */
    
    int clntSocket = *(int*)sock;
    
    replyToClient("Welcome, please send the RPN string\n", clntSocket);
    
    /* Receive message from client */
    if((bytesRcvd = recv(clntSocket, buffer, RCVBUFSIZE, 0)) < 0) {
        ExitWithError("recv() failed");
    }
    
    rcvd = malloc(sizeof(char) * bytesRcvd);
    strncpy(rcvd, buffer, bytesRcvd);
    printf("Received: %s\n", rcvd);
    
    while(clntSocket)  //While connection is open
    {
        rcvd = strtok(rcvd, "\r\n");
        double rpnVal = rpn(rcvd,clntSocket);
        
        if(rpn_error == false) {
            char retValue[TXBUFSIZE];
            sprintf(retValue, "The RPN value is: %d\n",(int)rpnVal);
            replyToClient(retValue, clntSocket);
        }
        //Have calc (or not) the rpn value and now close the connection
        printf("Closing Connection.\n");
        close(clntSocket);
        clntSocket = (int)NULL;
    }
    pthread_exit(0);
}


int main(int argc, const char * argv[]) {
    
    int servSock;                    /* Socket descriptor for server */
    int clntSock;                    /* Socket descriptor for client */
    struct sockaddr_in servAddr;     /* Local address */
    struct sockaddr_in clntAddr;     /* Client address */
    unsigned short servPort;         /* Server port */
    unsigned int clntLen;            /* Length of client address data structure */
    
    servPort = TCP_PORT;
    
    /* Create socket for incoming connections */
    if ((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        ExitWithError("Failed to create socket()");
    
    /* Construct local address structure */
    memset(&servAddr, 0, sizeof(servAddr));         /* Zero out structure */
    servAddr.sin_family = AF_INET;                  /* Internet address family */
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);   /* Any incoming interface */
    servAddr.sin_port = htons(servPort);            /* Local port */
    
    /* Bind to the local address */
    if (bind(servSock, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0)
        ExitWithError("Failed to bind()");
    
    /* Mark the socket so it will listen for incoming connections */
    if (listen(servSock, MAXPENDING) < 0)
        ExitWithError("Failed to listen()");
    
    printf("Waiting for incoming connection...\n");
    
    for (;;) /* Run forever */
    {
        /* Set the size of the in-out parameter */
        clntLen = sizeof(clntAddr);
        
        /* Wait for a client to connect */
        if ((clntSock = accept(servSock, (struct sockaddr *) &clntAddr,&clntLen)) < 0)
            ExitWithError("Failed to accept()");
        
#ifdef USE_THREAD
        pthread_t _thread;
        new_sock = malloc(1);
        *new_sock = clntSock;
        
        if( pthread_create( &_thread , NULL ,  HandleTCPClient , (void*) new_sock) < 0)
        {
            perror("Could not create thread.");
            exit(1);
        }
#else
        HandleTCPClient(clntSock);
#endif
        /* clntSock is connected to a client! */
        printf("Handling client %s\n", inet_ntoa(clntAddr.sin_addr));
    }
    return 0;
}
