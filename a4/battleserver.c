/*
 ============================================================================
 Name        : battle.c
 Author      : David London-Schacht and Farhan Samir
 Version     : 1
 Copyright   : copyright David London-Schacht 2015
 Description : CSC209 Assignment 4
 ============================================================================
 */

/*
 * socket demonstrations:
 * This is the server side of an "internet domain" socket connection, for
 * communicating over the network.
 *
 * In this case we are willing to wait either for chatter from the client
 * _or_ for a new connection.
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef PORT
    #define PORT 31300
#endif

#define MAX_SCORE  30
#define MIN_SCORE  20

struct client {
    int fd;
    struct in_addr ipaddr;
    struct client *next;
};


static struct client *addclient(struct client *top, int fd, struct in_addr addr);
static struct client *removeclient(struct client *top, int fd);
static void broadcast(struct client *top, char *s, int size);
int handleclient(struct client *p, struct client *top);
int generateRandom();


int bindandlisten(void);

int main(void) {
    int clientfd, maxfd, nready;
    struct client *p; //used for access to list
    struct client *head = NULL; //client list
    socklen_t len;
    struct sockaddr_in q;
    struct timeval tv;
    fd_set allset;
    fd_set rset;

    int i;
    int clientCount = 0;
    char buf[10]; //array for user to enter name

    // Create a new socket to allow communication 
    int listenfd = bindandlisten(); 

    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset); // set of file descriptors

    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    while (1) {
        // make a copy of the set before we pass it into select
        rset = allset;
        /* timeout in seconds (You may not need to use a timeout for
        * your assignment)*/
        tv.tv_sec = 10;
        tv.tv_usec = 0;  /* and microseconds */
        
        
        nready = select(maxfd + 1, &rset, NULL, NULL, &tv);
        if (nready == 0) {
             printf("No response from clients in %ld seconds\n", tv.tv_sec);
            continue;
         } 

        if (nready == -1) {
            perror("select");
            continue;
        }

        /* read username from STDIN */
        printf("What is your name?\n");
        read(STDIN_FILENO, buf, sizeof(buf) + 1);
        
        strtok(buf,"\n"); //remove newline char from buffer
        printf("Welcome, %s! ", buf);
        printf("Awaiting opponent...\n");


        /*next check if another client connected
        1) print to STDOUT "You engage clientname"
        2) call function to generate hitpoints and powermoves
        */

        if(clientCount == 2)
        {/* 2 connections eshablished */
        
            printf("%s\n" "**", buf ,"enters the arena**");
            printf("%s\n", "You engage", buf);
            //keep track of whos in the match 
            // read message decide what action to take
            // then write to other client 
            // turn variable 

        }
        

    


        if(FD_ISSET(listenfd, &rset)){
            //printf("a new client is connecting\n");
            len = sizeof(q); //accept connection of listenfd stream socket
            if ((clientfd = accept(listenfd, (struct sockaddr *)&q, &len)) < 0) {
                perror("accept");
                exit(1);
            }
            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            //printf("connection from %s\n", inet_ntoa(q.sin_addr));
            head = addclient(head, clientfd, q.sin_addr);
            clientCount++;
            // add newly established connection
        }

        for(i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &rset)) {
                for (p = head; p != NULL; p = p->next) {
                    if (p->fd == i) { //assigning client fd to i
                        int result = handleclient(p, head);
                        if (result == -1) {
                            int tmp_fd = p->fd;
                            head = removeclient(head, p->fd);
                            FD_CLR(tmp_fd, &allset);
                            close(tmp_fd);
                        }
                        break;
                    }
                }
            }
        }
    }
    return 0;
}

int handleclient(struct client *p, struct client *top) {
    char buf[256];
    char outbuf[512];
    int len = read(p->fd, buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        printf("Received %d bytes: %s", len, buf);
        sprintf(outbuf, "%s says: %s", inet_ntoa(p->ipaddr), buf); //print to buffer
        broadcast(top, outbuf, strlen(outbuf));
        return 0;
    } else if (len == 0) {
        // socket is closed
        printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
        sprintf(outbuf, "Goodbye %s\r\n", inet_ntoa(p->ipaddr));
        broadcast(top, outbuf, strlen(outbuf));
        return -1;
    } else { // shouldn't happen
        perror("read");
        return -1;
    }
}

 /* bind and listen, abort on error
  * returns FD of listening socket

  Bind system call binds a socket to an address
  */
int bindandlisten(void) {
    struct sockaddr_in r;
    int listenfd;
    // set up sockets
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    int yes = 1;
    if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }
    memset(&r, '\0', sizeof(r));
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(PORT);

    if (bind(listenfd, (struct sockaddr *)&r, sizeof r)) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }
    return listenfd;  //Return new socket.
}

static struct client *addclient(struct client *top, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        perror("malloc");
        exit(1);
    }

    printf("Adding client %s\n", inet_ntoa(addr));

    p->fd = fd;
    p->ipaddr = addr;
    p->next = top;
    top = p;
    return top;
}

static struct client *removeclient(struct client *top, int fd) {
    struct client **p;

    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                 fd);
    }
    return top;
}


static void broadcast(struct client *top, char *s, int size) {
    struct client *p;
    for (p = top; p; p = p->next) {
        write(p->fd, s, size);
    }
    /* should probably check write() return value and perhaps remove client */
}

/* 
===========================================================================
 
* Each player starts a match with between 20 and 30 hitpoints. 
    (Note that hitpoints and powermoves are reset to random values on the start of a new match,
    independent of what the values may have been following their previous match.)

* Each player starts a match with between one and three powermoves.
* Damage from a regular attack is 2-6 hitpoints.
* Powermoves have a 50% chance of missing. If they hit, 
then they cause three times the damage of a regular attack.
 
 ============================================================================
 */
// MAX_SCORE and MIN_SCORE macros defined above
int generateRandom(){

    int randomnumber = random() % (MAX_SCORE - MIN_SCORE + 1) + MIN_SCORE;
    return randomnumber;


}
// initialize powermoves and attacks
int initAttacks(){


}
int generatePowerMoves(){

}

void findMatch(struct client *a){



}
