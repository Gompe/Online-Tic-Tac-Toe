#ifndef SERVER_H
#define SERVER_H

#include <netinet/in.h>

#define MAX_SIZE 5000
#define MAX_CLIENTS 2
// #define INET_ADDRSTRLEN 1000

#define FYI 1
#define MYM 2
#define END 3
#define TXT 4
#define MOV 5
#define LFT 6

typedef struct game_state{

  int n_occupied;
  int player_to_move;
  int game_result;
  int is_game_over;
  char cells[3][3];

} game_state;

typedef struct udp_info{

  char buffer[MAX_SIZE];
  struct sockaddr_in client_addr;
  int n_bytes;
  socklen_t len;

} udp_info;

typedef struct game_message{

  int player_id;
  int processed;
  char code;
  char data[MAX_SIZE];

} game_message;

int listen_data(void);

void *handler(void *params);
int identify_client(const struct sockaddr_in *addr);

int parse_data(char *data, game_message *g_msg);
int is_game_message_valid(const game_message *g_msg);

void *game_loop(void *params);
void update_game_status(void);

void *initialize_game(void);
void *finalize_game(void);
void *send_information_messages(void);

void *send_data(udp_info *info);
void send_txt(struct sockaddr_in addr, char *message);

void print_bytes(void *ptr, int len);

#endif