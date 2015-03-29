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


struct client {
    int fd;
    char *name;
    struct in_addr ipaddr;
    struct client *next;
};


static struct client *addclient(struct client *top, int fd, struct in_addr addr);
static struct client *removeclient(struct client *top, int fd);
static void broadcast(struct client *top, char *s, int size);
int handleclient(struct client *p, struct client *top);
int generateHitPoints();
void computeDamage(int attack_points, int hit_points, char buffer[]);
int find_network_newline(char *buf, int inbuf);


int bindandlisten(void);

int main(void)
{
    printf("%d\n", PORT);
    int clientfd, maxfd, nready;
    struct client *p; //used for access to list
    struct client *head = NULL; //client list
    socklen_t len;
    struct sockaddr_in q;
    fd_set allset;
    fd_set rset;

    int i;
    printf("here\n");
    // Create a new socket to allow communication
    int listenfd = bindandlisten();

    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset); // set of file descriptors

    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    while (1)
    {
        // make a copy of the set before we pass it into select
        printf("here\n");
        rset = allset;
        /*select will wait until an exceptional event occurs when tv is NULL*/


        printf("here\n");
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        printf("%d\n", nready);
        if (nready == 0) {
            continue;
         }

        if (nready == -1) {
            perror("select");
            continue;
        }

        //FD_ISSET returns 1 when a new connection is attempted
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
            printf("connection from %s\n", inet_ntoa(q.sin_addr));
            //TODO: Adjust addclient (DONE)
            head = addclient(head, clientfd, q.sin_addr);

        }

        for(i = 0; i <= maxfd; i++)
        {
            if (FD_ISSET(i, &rset)) {
                for (p = head; p != NULL; p = p->next)
                {
                    if (p->fd == i)
                    { //assigning client fd to i
                        int result = handleclient(p, head);
                        if (result == -1)
                        {
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

int process_line(struct client *p, char *buf)
{
    int initial = 0;
    int nbytes;
    int inbuf; //how many bytes currently in buffer?
    int room; //how much room left in buffer
    char *after; //pointer to position after the data in buf
    int where; //location of network newline

    while (1)
    {
        inbuf = 0;  //buffer is empty; has no bytes
        room = sizeof(buf); //room == capacity of the whole buffer
        after = buf; // start writing at beginning of buf

        while ((nbytes = read(p->fd, after, room)) > 0)
        {
            initial++;
            inbuf = inbuf + nbytes;
            where = find_network_newline(buf, inbuf);

            if (where >= 0 && p->name == NULL)
            {
                buf[where] = '\n';
                buf[where + 1] = '\0';

                p->name = malloc(sizeof(buf));
                strncpy(p->name, buf, sizeof(buf));
            }
            room = sizeof(buf) - inbuf;
            after = buf + inbuf;

        }
        //socket is closed
        if (initial == 0 && nbytes == 0)
        {
            return -1;
        }

    }
    return 0;

}

int handleclient(struct client *p, struct client *top) {
    //the first thing they write will be their name
    //You will have to buffer the text that they enter
    //Using code from lab9.
    char buf[256];
    char outbuf[512];
    if (p->name == NULL)
    {
        int process_line_result = process_line(p, buf);
        char *message = "You are awaiting an opponent\r\n";
        write(p->fd, message, strlen(message) + 1);
        sprintf(outbuf, "%s has entered the arena!", p->name);
        broadcast(top, outbuf, strlen(outbuf));
        return process_line_result;
    }

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

    if (bind(listenfd, (struct sockaddr *)&r, sizeof(r))) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, SOMAXCONN)) {
        perror("listen");
        exit(1);
    }
    return listenfd;  //Return new socket.
}

static struct client *addclient(struct client *top, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));
    p->name = NULL;
    if (!p) {
        perror("malloc");
        exit(1);
    }
    p->fd = fd;
    p->ipaddr = addr;
    p->next = NULL;


    const char *intro_message = "What is your name? \r\n";
    int write_check = write(fd, intro_message, strlen(intro_message) + 1);
    if (write_check != strlen(intro_message) + 1)
    {
        perror("write(addclient)");
        exit(EXIT_FAILURE);
    }
    //TODO: Tell the new client that they are awaiting an opponent
    //TODO: Tell everyone else that someone new has entered the arena


    //Adding to back of the client list`1
    while (top->next != NULL)
    {
        top = top->next;
    }
    top->next = p;
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
* Lab 9 Functions, used for processing
* full lines of text
*/
int find_network_newline(char *buf, int inbuf) {
  // Step 1: write this function
  int i = 0;

  while ((buf[i] != '\0') && (i < inbuf))
  {
    if (buf[i] == '\r')
    {
      //network newline iff it is followed by '\n'
      if (buf[i + 1] == '\n')
      {
        //location of '\r'
        return i;
      }
    }
    i++;
  }
  return -1;
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
// MAX_SCORE and MIN_SCORE macros defined above
int generateHitPoints(){

    int randomnumber = random() % (MAX_SCORE - MIN_SCORE + 1) + MIN_SCORE;
    return randomnumber;


}

// initialize Regular Attack points in range 2-6
int generateAttacks(){

    int attackPoints = random() % (6 - 2 + 1) + 2;
    return attackPoints;


}

// initialize powermoves in range 1-3
int generatePowerMoves(){

    int numMoves = 1 + (random() % 3);
    return numMoves;

}

// Compute Damage from attack
void computeDamage(int attack_points, int hit_points, char buffer[]){

    //Powermoves have a 50% chance of missing.
    int accuracy = 1 + (random() % 2);
    int numPowermoves = generatePowerMoves();
    int damage = 0;
    char move[2];

    //NOTE: check entry is valid and discard invalid 
    if(strncmp(move,"p",sizeof(move)) == 0) // if STDIN is a powermove
    { 
       if(numPowermoves >=1)
       {
        numPowermoves -= 1;
            if(accuracy == 1)//Powermoves hits
            {   
                damage = (3 * attack_points);
                hit_points -= damage; //powermoves cause 3x the damage of a regular attack.
                printf("%s%s\n","you hit", buffer , "for", damage, "damage!");

            }else{
                printf("you missed!");
            }
        }

    // regular attack
    }else if(strncmp(move,"a",sizeof(move)) == 0)
    {
        hit_points -= attack_points; 
        printf("%s%s\n","you hit", buffer , "for", attack_points, "damage!");

    }
    
    
}

void displayMenu(){

}


void findMatch(struct client *a){



}
