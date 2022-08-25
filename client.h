#ifndef CLIENT_H
#define CLIENT_H

#include <netinet/in.h>

#define MAX_SIZE 5000

#define FYI 1
#define MYM 2
#define END 3
#define TXT 4
#define MOV 5
#define LFT 6

typedef struct udp_info {
  int sockfd;
  struct sockaddr *servaddr_ptr;
} udp_info;

void *send_message_to_server(int sockfd, const struct sockaddr *servaddr_ptr);
int read_message_from_server(int sockfd, struct sockaddr *servaddr_ptr);
void *user_input_manager(void *params);

#endif
