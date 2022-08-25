#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <assert.h>
#include <pthread.h>

#include "client.h"

#define DEBUG_MODE 1


int main(int argc, char *argv[]){

  /* checking the arguments from the command line */
  if (argc != 3) {
    printf("You need to pass arguments IP_ADDRESS and PORT_NUMBER\n");
    exit(-1);
  }

  int port;
  if (sscanf(argv[2], "%d", &port) != 1) {
    printf("Could not parse the arguments");
    exit(-1);
  }
  char *ip_addr = argv[1];

  /* creating socket */
  int sockfd;

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  } else {
    printf("Socket open.\n");
  }

  /* server information */
  struct sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(port);
  inet_pton(AF_INET, ip_addr, &servaddr.sin_addr.s_addr);

  /* data structure for using udp */
  udp_info info;
  info.sockfd = sockfd;
  info.servaddr_ptr = (struct sockaddr *)&servaddr;

  /* creating thread that receives user input */
  pthread_t thread_user_input;
  pthread_create(&thread_user_input, NULL, user_input_manager, (void *) &info);

  while (read_message_from_server(sockfd, (struct sockaddr *)&servaddr) == 0) {
    continue;
  }

  close(sockfd);

  return 0;
}

/* read_line - 
 *
 * Reads a line from the command line.
 */
int read_line(char *buffer, int buffsize)
{
  
  if (fgets(buffer, buffsize, stdin) == NULL) {
    printf("Input Error\n");
    exit(-1);
  }
  else {
    return strlen(buffer);
  }

  return 0;
}

/*
 *
 * user_input_manager - 
 * Keeps looping in order to check for user input. This function
 * should be ran on a second thread, so that the application can
 * run while the user writes his/her commands.
 *
 */
void *user_input_manager(void *params)
{

  udp_info *info = (udp_info *)params;
  int sockfd = info->sockfd;
  const struct sockaddr *servaddr_ptr = info->servaddr_ptr;

  while (1) {
    send_message_to_server(sockfd, servaddr_ptr);
  }
}

/*
 *
 * send_message_to_server - 
 * 
 * Waits for user to write a command in the terminal and press Enter.
 * 
 * Once the command is received, it parses the input and checks if it corresponds
 * to one of the valid commands: MOV, TXT.
 * 
 * If command is parsed successfully, sends the message to the server. Otherwise,
 * it prints in the terminal that parsing was not successful. 
 * 
 */
void *send_message_to_server(int sockfd, const struct sockaddr *servaddr_ptr) {
  char msg[MAX_SIZE];

  int msglen = read_line(msg, MAX_SIZE);

  if (msglen <= 3) {
    /* not a valid message code */
    printf("Could not parse instruction.\n");
    return NULL;
  }

  /* checking the type of message */
  if (msg[0] == 'M' && msg[1] == 'O' && msg[2] == 'V') {
    /* MOV message */
    int col, row;
    if (sscanf(msg+3, "%d%d", &col, &row) != 2) {
      /* is not a valid move */
      printf("Could not parse MOV - Try again.\n");
    } else {
      
      char msg_to_send[3];
      int len_msg_to_send = 3;

      msg_to_send[0] = MOV;
      msg_to_send[1] = (char) col;
      msg_to_send[2] = (char) row;

      sendto(sockfd, (const void *) msg_to_send, len_msg_to_send, 
              MSG_CONFIRM, servaddr_ptr, sizeof(*servaddr_ptr));

    }
  }

  else if (msg[0] == 'T' && msg[1] == 'X' && msg[2] == 'T') {
    /* TXT messages */
    char *msg_to_send = msg + 3;
    int len_msg_to_send = msglen - 3;

    msg_to_send[0] = TXT;
    msg_to_send[len_msg_to_send - 1] = (char) 0;

    sendto(sockfd, (const void *) msg_to_send, len_msg_to_send,
            MSG_CONFIRM, servaddr_ptr, sizeof(*servaddr_ptr));
  }

  else {
    printf("Message code not found.\n");
  }

  return NULL;
}

/*
 * read_message_from_server - 
 * 
 * Waits for the server to send a message. Once the message is received, 
 * deals with it properly and prints the relevant information in the terminal.
 * 
 * Accepted types of message:
 * 
 * TXT 0x04, MYM 0x02, END 0x03, FYI 0x01
 * 
 * RETURN: 
 *  Returns 1 if and only if the game has ended. Else returns 0.
 *
 */
int read_message_from_server(int sockfd, struct sockaddr *servaddr_ptr) {
  char buffer[MAX_SIZE];

  socklen_t len = sizeof(struct sockaddr);
  int n_bytes = recvfrom(sockfd, (char *)buffer, MAX_SIZE, MSG_WAITALL,
                                 servaddr_ptr, &len);

  if (n_bytes < 0) {
    perror("recvfrom");
    exit(1);
  }

  buffer[n_bytes] = '\0';

  int i;
  switch(buffer[0]){
    case TXT:
      /* TXT - prints message in the terminal */
      printf("[TXT]\n");
      printf("%s\n", buffer+1);
      break;

    case MYM:
      /* MYM - asks the user to make a move */
      printf("[MYM]\n");
      break;

    case END:
      /* END - prints the outcome of the game and returns 1 */
      printf("[END]\n");

      if (buffer[1] == (char) 0xff) {
        printf("There is no room for new participants.\n");
      } else if (buffer[1] == (char) 0){
        printf("Draw\n");
      } else {
        printf("Winner is player %d\n", (int) buffer[1]);
      }

      return 1;
      break;

    case FYI:
      /* FYI - prints the current state of the game in the terminal */
      printf("[FYI]\n");
      int n_occupied = (int) buffer[1];

      char cells[3][3];
      memset(cells, 0, 9*sizeof(char));

      for (i=0; i<n_occupied; ++i){
        int player, col, row;
        player = (int) buffer[2+3*i];
        col = (int) buffer[3+3*i];
        row = (int) buffer[4+3*i];

#if DEBUG_MODE
        assert(player==1 || player == 2);
        assert(0 <= col && col <= 2);
        assert(0 <= row && row <= 2);
#endif
        cells[row][col] = (player == 1) ? 'X' : 'O';
      }

      printf("%d filled positions.\n", n_occupied);

      printf("\n");
      printf("+-+-+-+\n");
      int row, col;
      for(row=0; row<3; ++row){
        printf("|");
        for(col=0; col<3; ++col){
          printf("%c|", cells[row][col] ? cells[row][col] : ' ');
        }
        printf("\n");
        printf("+-+-+-+\n");
      }
      break;
  
    default:
      /* If the message cannot be identified */
      printf("Message code not found.\n");
      break;
  }

  return 0;
}