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

struct message_manager {
    char message_buffer[256];
    int room;
    char *after;
    int inbuf;
};

struct client {
    int fd;
    char name[256];
    struct in_addr ipaddr;
    struct client *next;
    struct message_manager message;
    int in_battle;
    int last_faced_fd;

};



static struct client *addclient(struct client *top, int fd, struct in_addr addr);
static struct client *removeclient(struct client *top, int fd);
static void broadcast(struct client *top, char *s, int size);
int handleclient(struct client *p, struct client *top);
int generateHitPoints();
void computeDamage(int attack_points, int hit_points, char buffer[]);
int find_network_newline(char *buf, int inbuf);
int process_message(struct client *p);
int look_for_opponent(struct client *top, struct client *p);


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
        rset = allset;
        /*select will wait until an exceptional event occurs when tv is NULL*/


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

/*
* Does buffering
* Returns -1 upon an error or on socket being closed or message too big
* otherwise 0
*/
int process_message(struct client *p)
{
    int where;
    int first_iteration = 0;
    int nbytes; 
    int return_value = 0;
    while (1)
    {   
        nbytes = read(p->fd, (p->message).after, (p->message).room);
        if (nbytes <= 0)
        {
            break;
        }
        first_iteration = 1;
        (p->message).inbuf = (p->message).inbuf + nbytes;
        where = find_network_newline((p->message).message_buffer, (p->message).inbuf);
        if (where >= 0)
        {
            (p->message).message_buffer[where] = '\n';
            (p->message).message_buffer[where + 1] = '\0';


            if ((p->name)[0] == '\0')
            {
                (p->message).message_buffer[where] = '\0';
                strncpy(p->name, (p->message).message_buffer, sizeof(p->message).message_buffer);
                return_value = 1;
            }
            else
            {
                return_value = 2;
            }
            (p->message).inbuf -= where + 2;
            memmove((p->message).message_buffer, (p->message).message_buffer + where + 2, sizeof(p->message).message_buffer);
        }
        (p->message).room = sizeof((p->message).message_buffer) - (p->message).inbuf;
        (p->message).after = (p->message).message_buffer + (p->message).inbuf;
        if (return_value == 1 || return_value == 2)
        {
            break;
        }

        //The user entered more than 256 characters for a message
        //They will be deleted from the client list because this is a violation
        if ((p->message).room == 0 && where < 0)
        {
            return -1;
        }

    }

    if (first_iteration == 0)
    {
        return -1;
    }

    return return_value;
}
/*
* Returns 1 if an opponent is found
* Otherwise, returns 0
*/
int look_for_opponent(struct client *top, struct client *p)
{   
    struct client *iterator;
    for (iterator = top; iterator != NULL; iterator = iterator -> next)
    {
        if (iterator->in_battle == 0 && iterator->last_faced_fd != p->fd && (iterator->name)[0] != '\0')
        {
            iterator->in_battle = 1;
            p->in_battle = 1;
            iterator->last_faced_fd = p->fd;
            p->fd = iterator->fd;
            return 1;
        }
    }
    return 0;
}


int handleclient(struct client *p, struct client *top) 
{   
    int process_message_result = process_message(p);
    //Error or socket was closed, delete the client
    if (process_message_result == -1)
    {
        printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
        return process_message_result;
    }

    if (process_message_result == 1)
    {
        char *message = "You are awaiting an opponent\r\n";
        write(p->fd, message, strlen(message) + 1);
        char outbuf[512];
        sprintf(outbuf, "%s has joined the arena\n", p->name);
        broadcast(top, outbuf, strlen(outbuf) + 1);
        int search_result = look_for_opponent(top, p);
        if (search_result == 1)
        {

        }
    }

    return process_message_result;
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
    if (!p) {
        perror("malloc");
        exit(1);
    }
    (p->message).inbuf = 0;
    (p->message).after = (p->message).message_buffer;
    (p->message).room = sizeof((p->message).message_buffer);
    p->fd = fd;
    p->ipaddr = addr;
    p->next = NULL;
    p->in_battle = 0;
    p->last_faced_fd = 0;
    (p->name)[0] = '\0';


    const char *intro_message = "What is your name? \r\n";
    int write_check = write(fd, intro_message, strlen(intro_message) + 1);
    if (write_check != strlen(intro_message) + 1)
    {
        perror("write(addclient)");
        exit(EXIT_FAILURE);
    }

    if (top == NULL)
    {
        top = p;
        return top;
    }
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

*/
