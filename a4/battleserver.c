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

#define MAX_SCORE 30
#define MIN_SCORE 20

/*
* Used to handle messages (such as name and speaking to an opponent)
* and commands: (a)ttack, (p)owermove and (s)peak
*/
struct message_manager {
    char message_buffer[256];
    int room;
    char *after;
    int inbuf;

    char command_buffer[60];
    int command_room;
    char *command_after;
    int command_inbuf;
};

/*
* Used to manage a match for a client including a pointer to the 
* client that they are facing, the file descriptor of the client 
* they faced in the previous match, a boolean variable that 
* indicates whether they are in a match or not, the hitpoints 
* and the number of powermoves they have
*/
struct match {
    struct client *currently_facing;
    int past_fd;
    int in_match;
    int hp;
    int powermoves;
};

/*
* - fd is the file descriptor for the client (or socket)
* - must_process is a boolean variable that indicates
*   a client in a match has spoken something after
*   entering the (s) command and it must be 
*   processed (i.e written to the opponent)
* - turn is a boolean variable that indicates
*   if it is their turn or not in a match,
*   it is set to 0 when they are not in a match
* - Each client has a struct message and a struct
*   match to manage messages and matches. 
*/
struct client {
    int fd;
    int must_process;
    int turn;
    char name[256];
    struct in_addr ipaddr;
    struct client *next;
    struct message_manager message;
    struct match combat;

};


/*
* Adds a client to the list of clients currently in the game and
* returns the new list. 
*/
struct client *addclient(struct client *top, int fd, struct in_addr addr);

/*
* Removes a client from the list of clients currently in the game and
* returns the new list
*/
struct client *removeclient(struct client *top, int fd);

/*
* Writes a message to every player currently in the game
*/
void broadcast(struct client *top, char *s, int size);

/*
* Only called when select notices that the client is ready to be handled and 
* performs the necessary actions based on the client's actions and state. 
*/
int handleclient(struct client *p, struct client *top);

/*
* Randomly generates hitpoints for a client for the match. 
* The number of hitpoints is a random number between 20 and 30 (inclusive)
*/
int generatehp();


/*
* Looks for a network new line given a line of text and returns the index of the '\r' 
*/
int find_network_newline(char *buf, int inbuf);

/*
* Processes a message and either stores it as the client's name if
* the client has yet to enter a name or writes it to the opponent client
* in a match. 
*/
int process_message(struct client *p);

/*
* Looks for a suitable opponent for a client.
*/
int look_for_opponent(struct client *top, struct client *p);

/*
* Prints client's hitpoints, number of powermoves and opponent client's hitpoints
*/
int print_stats(struct client *p1, struct client *p2);

/*
* Prints a list of options (attack, speak and powermoves) or states that they are waiting
* for the opposing client to do something.
*/
int print_options(struct client *p1, struct client *p2);

/*
* Randomly generates the number of powermoves that a client can use in a match. The
* number is between 1 and 3 (inclusive)
*/
int generatepowermoves();

/*
* Creates a new socket to allow communication between clients
*/
int bindandlisten(void);

/*
* Reads and discards any useless text that the client enters 
* (such as typing when it is not their turn)
*/
int read_and_discard(struct client *p);

/*
* Computes damage from an attack (normal attack and powermove) and 
* decrements a client's hitpoints accordingly. Also 
* ends the match if all hitpoints are gone for a client and 
* attempts to start a new one. 
*/
int compute_damage(struct client *top, struct client *p1, struct client *p2, int attack_type);

/*
* Processes a command entered by a client while in a match 
* (a for attack, s for speak or p for powermove)
*/
int process_command(struct client *p, struct client *top);

/*
* Searches for a valid command (a, p, s) in a line of text and returns the index of the command.
*/
int find_command(char *buf, int inbuf, int powermoves_left);

/*
* Randomly generates number of points in a normal attack. 
* The number is between 2 and 6 (inclusive)
*/
int normal_attack();

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
    //Stores the index of the network newline in a string, or -1 if it doesn't exist
    int where;

    /*The purpose of the first iteration variable is if read returns 0 upon the 
    * first iteration of the while loop, then we know that the client has exited
    * But if it returns 0 afterwards, then we know that there was nothing else left to read,
    * not that the client has exited. 
    */
    int first_iteration = 0;
    int nbytes; 

    /*Returns 1 if the user is entering a name
    * Returns 2 if the user is communicating to the opponent after
    * entering (s) when it is their turn
    * Returns -1 if some sort of error occurs
    * Never returns 0. 
    */
    int return_value = 0;
    while (1)
    {   

        nbytes = read(p->fd, (p->message).after, (p->message).room);
        if (nbytes == -1)
        {
            perror("read");
            return -1;
        }

        //No purpose in continuing read, either reading is complete or the user quit. 
        if (nbytes == 0)
        {
            break;
        }

        //If we get past this point, then the user has not quit and so first_iteration can be set to 1
        first_iteration = 1;

        //From lab 9
        (p->message).inbuf = (p->message).inbuf + nbytes;

        //also from lab 9
        where = find_network_newline((p->message).message_buffer, (p->message).inbuf);
        if (where >= 0)
        {
            (p->message).message_buffer[where] = '\n';
            (p->message).message_buffer[where + 1] = '\0';

            //This means that their name has not been set, so it should be set now. 
            if ((p->name)[0] == '\0')
            {
                (p->message).message_buffer[where] = '\0';
                strncpy(p->name, (p->message).message_buffer, sizeof(p->message).message_buffer);
                return_value = 1;
            }
            //Otherwise, they had pressed the speak command when it was their turn and
            //this processes what they said. 
            else
            {   int write_check;
                char outbuf[512];
                sprintf(outbuf, "%s takes a break to tell you: \r\n", p->name);
                write_check = write(((p->combat).currently_facing)->fd, outbuf, strlen(outbuf) + 1);
                if (write_check == -1)
                {
                    perror("write");
                    return -1;
                }
                //Writes the message they want to send to their opponent's file descriptor
                strncpy(outbuf, (p->message).message_buffer, sizeof(p->message).message_buffer);
                write_check = write(((p->combat).currently_facing)->fd, outbuf, strlen(outbuf) + 1);
                if (write_check == -1)
                {
                    perror("write");
                    return -1;
                }
                return_value = 2;
            }
            (p->message).inbuf -= where + 2;
            memmove((p->message).message_buffer, (p->message).message_buffer + where + 2, sizeof(p->message).message_buffer);
        }
        (p->message).room = sizeof((p->message).message_buffer) - (p->message).inbuf;
        (p->message).after = (p->message).message_buffer + (p->message).inbuf;

        //We processed their name or message and therefore we don't have to read anymore. 
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

        break;

    }

    if (first_iteration == 0)
    {   

        return -1;
    }

    return return_value;
}


int print_stats(struct client *p1, struct client *p2)
{
    char outbuf[512];
    int write_check;

    sprintf(outbuf, "Your hitpoints: %d\r\n", (p1->combat).hp);
    write_check = write(p1->fd, outbuf, strlen(outbuf) + 1);
    if (write_check == -1)
    {
        perror("write");
        return -1;
    }
    sprintf(outbuf, "Your powermoves: %d\n\r\n", (p1->combat).powermoves);
    write_check = write(p1->fd, outbuf, strlen(outbuf) + 1);
    if (write_check == -1)
    {
        perror("write");
        return -1;
    }
    sprintf(outbuf, "%s's hitpoints: %d\r\n", p2->name, (p2->combat).hp);
    write_check = write(p1->fd, outbuf, strlen(outbuf) + 1);
    if (write_check == -1)
    {
        perror("write");
        return -1;
    }
    sprintf(outbuf, "Your hitpoints: %d\r\n", (p2->combat).hp);
    write_check = write(p2->fd, outbuf, strlen(outbuf) + 1);
    if (write_check == -1)
    {
        perror("write");
        return -1;
    }
    sprintf(outbuf, "Your powermoves: %d\n\r\n", (p2->combat).powermoves);
    write_check = write(p2->fd, outbuf, strlen(outbuf) + 1);
    if (write_check == -1)
    {
        perror("write");
        return -1;
    }
    sprintf(outbuf, "%s's hitpoints: %d\r\n", p1->name, (p1->combat).hp);
    write_check = write(p2->fd, outbuf, strlen(outbuf) + 1);
    if (write_check == -1)
    {
        perror("write");
        return -1;
    }
    return 0;
}

int print_options(struct client *p1, struct client *p2)
{
    char outbuf[512];
    int write_check;
    if (p1->turn == 1)
    {
        sprintf(outbuf, "\n(a)ttack\r\n");
        write_check = write(p1->fd, outbuf, strlen(outbuf) + 1);
        if (write_check == -1)
        {
            perror("write");
            return -1;
        }
        if ((p1->combat).powermoves > 0)
        {
            sprintf(outbuf, "(p)owermove\r\n");
            write_check = write(p1->fd, outbuf, strlen(outbuf) + 1);
            if (write_check == -1)
            {
                perror("write");
                return -1;
            }
        }
        sprintf(outbuf, "(s)peak something\r\n");
        write_check = write(p1->fd, outbuf, strlen(outbuf) + 1);
        if (write_check == -1)
        {
            perror("write");
            return -1;
        }
        sprintf(outbuf, "waiting for %s to strike...\n\r\n", p1->name);
        write_check = write(p2->fd, outbuf, strlen(outbuf) + 1);
        if (write_check == -1)
        {
            perror("write");
            return -1;
        }
    }
    else
    {   
        sprintf(outbuf, "\n(a)ttack\r\n");
        write_check = write(p2->fd, outbuf, strlen(outbuf) + 1);
        if (write_check == -1)
        {
            perror("write");
            return -1;
        }
        if ((p2->combat).powermoves > 0)
        {
            sprintf(outbuf, "(p)owermove\r\n");
            write_check = write(p2->fd, outbuf, strlen(outbuf) + 1);
            if (write_check == -1)
            {
                perror("write");
                return -1;
            }
        }
        sprintf(outbuf, "(s)peak something\r\n");
        write_check = write(p2->fd, outbuf, strlen(outbuf) + 1);
        if (write_check == -1)
        {
            perror("write");
            return -1;
        }
        sprintf(outbuf, "waiting for %s to strike...\n\r\n", p2->name);
        write_check = write(p1->fd, outbuf, strlen(outbuf) + 1);
        if (write_check == -1)
        {
            perror("write");
            return -1;
        }
    }
    return 0;
}

int find_command(char *buf, int inbuf, int powermoves_left)
{
    int i = 0;
    while ((buf[i] !='\0') && (i < inbuf))
    {
        if ((buf[i] == 'a') || (buf[i] == 'p' && powermoves_left > 0) || (buf[i] == 's'))
        {
            return i;
        } 
        i++;   
    }
    return -1;

}
int normal_attack()
{
    return (random() % 5) + 2;
}


int compute_damage(struct client *top, struct client *p1, struct client *p2, int attack_type)
{   
    char outbuf[512];
    int attack_damage = normal_attack();
    p1->turn = 0;
    int write_check;
    //Normal attack
    if (attack_type == 0)
    {
        sprintf(outbuf, "\nYou hit %s for %d damage!\r\n", p2->name, attack_damage);
        write_check = write(p1->fd, outbuf, strlen(outbuf) + 1);
        if (write_check == -1)
        {
            perror("write");
            return -1;
        }
        sprintf(outbuf, "\n%s hits you for %d damage!\r\n", p1->name, attack_damage);
        write_check = write(p2->fd, outbuf, strlen(outbuf) + 1);
        if (write_check == -1)
        {
            perror("write");
            return -1;
        }
        (p2->combat).hp = (p2->combat).hp - attack_damage;
        if ((p2->combat).hp  <= 0)
        {   
            sprintf(outbuf, "%s gives up. You win!\r\n", p2->name);
            write_check = write(p1->fd, outbuf, strlen(outbuf) + 1);
            if (write_check == -1)
            {
                perror("write");
                return -1;
            }
            sprintf(outbuf, "You are no match for %s. You scurry away...\r\n", p1->name);
            write_check = write(p2->fd, outbuf, strlen(outbuf) + 1);
            if (write_check == -1)
            {
                perror("write");
                return -1;
            }
            (p1->combat).in_match = 0;
            (p2->combat).in_match = 0;
            sprintf(outbuf, "\nAwaiting next opponent...\r\n");
            write_check = write(p1->fd, outbuf, strlen(outbuf) + 1);
            if (write_check == -1)
            {
                perror("write");
                return -1;
            }
            write_check = write(p2->fd, outbuf, strlen(outbuf) + 1);
            if (write_check == -1)
            {
                perror("write");
                return -1;
            }
            int search_result_one = look_for_opponent(top, p1);
            int search_result_two = look_for_opponent(top, p2);
            if (search_result_one == -1 || search_result_two == -1)
            {
                printf("Error occurred while looking for an opponent");
                return -1;
            }
            if (search_result_one == 1)
            {
                print_stats(p1, (p1->combat).currently_facing);
                print_options(p1, (p1->combat).currently_facing);
            }
            if (search_result_two == 1)
            {
                print_stats(p2, (p2->combat).currently_facing);
                print_options(p2, (p2->combat).currently_facing);
            }
            return 0;
        }
        p2->turn = 1;
        print_stats(p1, p2);
        print_options(p1, p2);
    }
    else
    {   
        (p1->combat).powermoves = (p1->combat).powermoves - 1;
        int accuracy = (random() % 2);
        if (accuracy == 1)
        {
            int power_damage = 3 * attack_damage;
            sprintf(outbuf, "\nYou hit %s for %d damage\r\n", p2->name, power_damage);
            write_check = write(p1->fd, outbuf, strlen(outbuf) + 1);
            if (write_check == -1)
            {
                perror("write");
                return -1;
            }
            sprintf(outbuf, "\n%s hit you for %d damage\r\n", p1->name, power_damage);
            write_check = write(p2->fd, outbuf, strlen(outbuf) + 1);
            if (write_check == -1)
            {
                perror("write");
                return -1;
            }
            (p2->combat).hp = (p2->combat).hp - power_damage;
            if ((p2->combat).hp  <= 0)
            {
                sprintf(outbuf, "%s gives up. You win!\r\n", p2->name);
                write_check = write(p1->fd, outbuf, strlen(outbuf) + 1);
                if (write_check == -1)
                {
                    perror("write");
                    return -1;
                }
                sprintf(outbuf, "You are no match for %s. You scurry away...\r\n", p1->name);
                write_check = write(p2->fd, outbuf, strlen(outbuf) + 1);
                if (write_check == -1)
                {
                    perror("write");
                    return -1;
                }
                (p1->combat).in_match = 0;
                (p2->combat).in_match = 0;
                sprintf(outbuf, "\nAwaiting next opponent...\r\n");
                write_check = write(p1->fd, outbuf, strlen(outbuf) + 1);
                if (write_check == -1)
                {
                    perror("write");
                    return -1;
                }
                write_check = write(p2->fd, outbuf, strlen(outbuf) + 1);
                if (write_check == -1)
                {
                    perror("write");
                    return -1;
                }
                int search_result_one = look_for_opponent(top, p1);
                int search_result_two = look_for_opponent(top, p2);
                if (search_result_one == -1 || search_result_two == -1)
                {
                    printf("An error occurred while searching for an opponent");
                    return -1;
                }
                if (search_result_one == 1)
                {
                    print_stats(p1, (p1->combat).currently_facing);
                    print_options(p1, (p1->combat).currently_facing);
                }
                if (search_result_two == 1)
                {
                    print_stats(p2, (p2->combat).currently_facing);
                    print_options(p2, (p2->combat).currently_facing);
                }
                return 0;
            }
            else
            {   
                p2->turn = 1;
                print_stats(p1, p2);
                print_options(p1, p2);
            } 
        }
        else
        {
            sprintf(outbuf, "\nYou missed!\r\n");
            write_check = write(p1->fd, outbuf, strlen(outbuf) + 1);
            if (write_check == -1)
            {
                perror("write");
                return -1;
            }
            sprintf(outbuf, "\n%s missed you!\r\n", p1->name);
            write_check = write(p2->fd, outbuf, strlen(outbuf) + 1);
            if (write_check == -1)
            {
                perror("write");
                return -1;
            }
            p2->turn = 1;
            print_stats(p1, p2);
            print_options(p1, p2);
        }
    }
    return 0;
}

int process_command(struct client *p, struct client *top)
{
    int where;
    int attack_type;
    int first_iteration = 0;
    int nbytes;
    char outbuf[512];
    

    while (1)
    {
        nbytes = read(p->fd, (p->message).command_after, (p->message).command_room);
        if (nbytes == -1)
        {
            perror("read");
            return -1;
        }
        if (nbytes == 0)
        {
            break;
        }
        first_iteration = 1;
        (p->message).command_inbuf = (p->message).command_inbuf + nbytes;
        where = find_command((p->message).command_buffer, (p->message).command_inbuf, (p->combat).powermoves);

        if (where >= 0)
        {
            if ((p->message).command_buffer[where] == 'a')
            {   
                attack_type = 0;
                return compute_damage(top, p, ((p->combat).currently_facing), attack_type);

            }
            else if ((p->message).command_buffer[where] == 'p' && (p->combat).powermoves > 0)
            {
                attack_type = 1;
                return compute_damage(top, p, ((p->combat).currently_facing), attack_type);
            }
            else
            {   
                int write_check;
                (p->message).inbuf = 0;
                (p->message).room = sizeof((p->message).message_buffer);
                (p->message).after = (p->message).message_buffer;
                sprintf(outbuf, "\nSpeak: \r\n");
                write_check = write(p->fd, outbuf, strlen(outbuf) + 1);
                if (write_check == -1)
                {
                    perror("write");
                    return -1;
                }
                p->must_process = 1;

            }
            (p->message).command_inbuf = 0;
            (p->message).command_room = sizeof((p->message).command_buffer);
            (p->message).command_after = (p->message).command_buffer;

        }

        if ((p->message).command_room == 0 && where == -1)
        {
            return -1;
        }

        break;
    }

    if (first_iteration == 0)
    {
        return -1;
    }

    return 0;
}


int handleclient(struct client *p, struct client *top) 
{   char outbuf[512];
    if (p->must_process == 1)
    {
        int speak_result = process_message(p);
        if (speak_result == 2)
        {
            p->must_process = 0;
        }
        return speak_result;

    }
    if ((p->combat).in_match == 1 && p->turn == 1)
    {
        return process_command(p, top);
    }

    if (p->turn == 0 && p->name[0] != '\0')
    {
        return read_and_discard(p);
    }

    int process_message_result = process_message(p);
    //Error or socket was closed, delete the client
    if (process_message_result == -1)
    {
        printf("Disconnect from %s\n", inet_ntoa(p->ipaddr));
        return process_message_result;
    }

    if (process_message_result == 1)
    {   
        int write_check;
        char *message = "You are awaiting an opponent\r\n";
        write_check = write(p->fd, message, strlen(message) + 1);
        if (write_check == -1)
        {
            perror("write");
            return -1;
        }
        
        sprintf(outbuf, "\n%s has joined the arena\r\n", p->name);
        broadcast(top, outbuf, strlen(outbuf) + 1);
        int search_result = look_for_opponent(top, p);
        if (search_result == -1)
        {
            return search_result;
        }
        if (search_result == 1)
        {   
            print_stats(p, (p->combat).currently_facing);
            print_options(p, (p->combat).currently_facing);
        }
    }


    return process_message_result;
}

int read_and_discard(struct client *p)
{
    int first_iteration = 0;
    int nbytes; 
    

    while (1)
    {
        nbytes = read(p->fd, (p->message).after, (p->message).room);
        if (nbytes == -1)
        {
            perror("read");
            return -1;
        }
        if (nbytes ==0)
        {
            break;
        }
        first_iteration = 1;
        (p->message).inbuf = (p->message).inbuf + nbytes;

        (p->message).room = sizeof((p->message).message_buffer) - (p->message).inbuf;
        if ((p->message).room > 0)
        {
            (p->message).room = sizeof((p->message).message_buffer);
            (p->message).after = (p->message).message_buffer;
            (p->message).inbuf = 0;
            break;
        }
        (p->message).room = sizeof((p->message).message_buffer);
        (p->message).after = (p->message).message_buffer;
        (p->message).inbuf = 0;

    }   
    if (first_iteration == 0)
    {
        return -1;
    }

    return 0;
}


int look_for_opponent(struct client *top, struct client *p)
{   
    char message[300];
    struct client *iterator;
    for (iterator = top; iterator != NULL; iterator = iterator->next)
    {
        if ((iterator->combat).in_match == 0 && (((iterator->combat).past_fd != p->fd) || ((p->combat).past_fd != iterator->fd)) && (iterator->name)[0] != '\0' && iterator->fd != p->fd)
        {   
            int write_check;
            (iterator->combat).in_match = 1;
            (p->combat).in_match = 1;
            (iterator->combat).currently_facing = p;
            (p->combat).currently_facing = iterator;
            (iterator->combat).past_fd = p->fd;
            (p->combat).past_fd = iterator->fd;
            (iterator->combat).hp = generatehp();
            (p->combat).hp = generatehp();
            (iterator->combat).powermoves = generatepowermoves();
            (p->combat).powermoves = generatepowermoves();
            p->turn = 1;
            sprintf(message, "You engage %s\r\n", p->name);
            write_check = write(iterator->fd, message, strlen(message) + 1);
            if (write_check == -1)
            {
                perror("write");
                return -1;
            }
            sprintf(message, "You engage %s\r\n", iterator->name);
            write_check = write(p->fd, message, strlen(message) + 1);
            if (write_check == -1)
            {
                perror("write");
                return -1;
            }
            return 1;
        }
    }
    return 0;
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

struct client *addclient(struct client *top, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));
    struct client *tmp = top;
    if (!p) {
        perror("malloc");
        exit(1);
    }
    p->must_process = 0;

    (p->message).inbuf = 0;
    (p->message).after = (p->message).message_buffer;
    (p->message).room = sizeof((p->message).message_buffer);

    (p->message).command_inbuf = 0;
    (p->message).command_after = (p->message).command_buffer;
    (p->message).command_room = sizeof((p->message).command_buffer);
    p->fd = fd;
    p->turn = 0;
    p->ipaddr = addr;
    p->next = NULL;
    (p->name)[0] = '\0';
    (p->combat).currently_facing = NULL;
    (p->combat).past_fd = p->fd;
    (p->combat).in_match = 0;
    (p->combat).hp = 0;
    (p->combat).powermoves = 0;


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
    while (tmp->next != NULL)
    {
        tmp = tmp->next;
    }
    tmp->next = p;
    return top;
}

struct client *removeclient(struct client *top, int fd) {
    struct client **p;

    for (p = &top; *p && (*p)->fd != fd; p = &(*p)->next)
        ;
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        char outbuf[512];
        if (((*p)->combat).in_match == 1)
        {   
            int write_check;
            sprintf(outbuf, "\n--%s dropped. You win!\n\r\n", (*p)->name);
            write_check = write((((*p)->combat).currently_facing)->fd, outbuf, strlen(outbuf) + 1);
            if (write_check == -1)
            {
                perror("write");
                exit(1);
            }
            (((*p)->combat).currently_facing)->turn = 0;
            ((((*p)->combat).currently_facing)->combat).in_match = 0;
            ((((*p)->combat).currently_facing)->combat).past_fd = 0;
            sprintf(outbuf, "\nAwaiting next opponent...\r\n");
            write_check = write((((*p)->combat).currently_facing)->fd, outbuf, strlen(outbuf) + 1);
            if (write_check == -1)
            {
                perror("write");
                exit(1);
            }
            int look_result = look_for_opponent(top, ((((*p)->combat).currently_facing)));
            if (look_result == 1)
            {
                print_stats(((*p)->combat).currently_facing, ( (((*p)->combat).currently_facing)->combat).currently_facing);
                print_options(((*p)->combat).currently_facing, ( (((*p)->combat).currently_facing)->combat).currently_facing);
            }
        }
        sprintf(outbuf, "\n**%s leaves **\r\n", (*p)->name);
        broadcast(top, outbuf, strlen(outbuf) + 1);
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


void broadcast(struct client *top, char *s, int size) 
{
    struct client *p;
    int write_check;
    for (p = top; p; p = p->next)
    {   
        write_check = write(p->fd, s, size);
        if (write_check == -1)
        {
            perror("write");
        }
    }
   
}

/*
* Lab 9 Functions, used for processing
* full lines of text
*/
int find_network_newline(char *buf, int inbuf) 
{
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

int generatehp(void)
{

    int randomnumber = random() % (MAX_SCORE - MIN_SCORE + 1) + MIN_SCORE;
    return randomnumber;
}

int generatepowermoves(void)
{

    int numMoves = 1 + (random() % 3);
    return numMoves;
}

