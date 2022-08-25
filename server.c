#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <assert.h>

#include "server.h"

#define DEBUG_MODE 1


int sockfd;


/* current players */
int n_connected_clients = 0;
struct sockaddr_in players[2];

/* game data */
game_state game;
game_message last_move;

/* multithreading */
pthread_mutex_t game_mutex;
pthread_cond_t new_move_cond;
pthread_cond_t new_player_cond;

int main(int argc, char **argv){

  /* checking command line arguments */
  if (argc != 2) {
    printf("You should pass exactly one argument to the program: PORT_NUMBER\n");
    exit(-1);
  }

  int port;
  if (sscanf(argv[1], "%d", &port) != 1) {
    printf("Could not parse the arguments");
    exit(-1);
  }

  /* init socket */
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket creation failed");
    exit(1);
  } else {
    printf("Socket Created.\n");
  }

  /* setting server address and binding socket */
  struct sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof(servaddr));

  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(port);

  if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    perror("bind failed");
    exit(1);
  } else {
    printf("Bind to port %d.\n", port);
  }

  /* initializing the thread that will be responsible for
    the game loop */
  pthread_mutex_init(&game_mutex, NULL);
  pthread_t game_thread; 
  if (pthread_create(&game_thread, NULL, game_loop, NULL)) {
    fprintf(stderr, "Could not create game thread.\n");
    exit(1);
  }

  /* thread responsible for listening to
    user interactions */
  if(listen_data()){
    fprintf(stderr, "Fatal error in listen()\n");
    exit(1);
  }

  return 0;
}

/* listen_data
 * 
 * Continuously listen to data. Once data is received,
 * passes it to the handler in a new thread.
 * 
 */
int listen_data(void){

  printf("Waiting for connections...\n");

  while(1){

    udp_info *info_ptr = (udp_info *)(malloc(sizeof(udp_info)));

    if (info_ptr == NULL) {
      fprintf(stderr, "Malloc Error\n");
      free(info_ptr);
      return 1;
    }

    memset(&info_ptr->client_addr, 0, sizeof(info_ptr->client_addr));
    info_ptr->len = sizeof(struct sockaddr);

    info_ptr->n_bytes = recvfrom(sockfd, (char *)info_ptr->buffer, MAX_SIZE, MSG_WAITALL,
                          (struct sockaddr *)&info_ptr->client_addr, &info_ptr->len);


    if (info_ptr->n_bytes < 0 || info_ptr->n_bytes >= MAX_SIZE){
      perror("recv error");
      free(info_ptr);
      return 1;
    }
    else{

#if DEBUG_MODE
      /* found here the instructions to print IP address */
      // https://stackoverflow.com/questions/9590529/how-should-i-print-server-address
      char buffer[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &info_ptr->client_addr.sin_addr, buffer, INET_ADDRSTRLEN);

      printf("+-----------------------------+\n");
      printf("Receiving Data from: %s::%d\n", buffer, htons(info_ptr->client_addr.sin_port));
      print_bytes((void *)info_ptr->buffer, info_ptr->n_bytes);
#endif

      info_ptr->buffer[info_ptr->n_bytes] = '\0';
      pthread_t handler_thread;
      pthread_create(&handler_thread, NULL, handler, (void *)info_ptr);      
    }
  }
}

void *handler(void *params){

  udp_info *info = (udp_info *)(params);
  /* checks if client is new or is one of the players */
  int client_id = identify_client(&info->client_addr);

  /* cases */
  if(n_connected_clients==2 && client_id==2){
    /* game no longer supports any new client */
    /* refuse new client */
  
    udp_info info_ans;
    info_ans.client_addr = info->client_addr;
    info_ans.len = info->len;

    info_ans.buffer[0] = END;
    info_ans.buffer[1] = 0xff;

    info_ans.n_bytes = 2;

    send_data(&info_ans);
  }

  else if(n_connected_clients < 2 && client_id == 2){
    /* new player contacted the server and server supports a new client */
    game_message g_msg;
    parse_data(info->buffer, &g_msg);

    if(g_msg.code == TXT && !strncmp(g_msg.data, "Hello\0", 6)){
      /* checks if the client requested to join the game */
      printf("+-----------------------------+\n");
      printf("Player %d assigned.\n", n_connected_clients + 1);

      /* send welcome message to client */
      char welcome_msg[MAX_SIZE];
      snprintf(welcome_msg, MAX_SIZE, "Wellcome! You are player %d. You play with %c.", n_connected_clients+1, n_connected_clients ? 'O' : 'X');
      send_txt(info->client_addr, welcome_msg);

      /* tells the game loop that a new client has joined the game */
      pthread_mutex_lock(&game_mutex);
      players[n_connected_clients] = info->client_addr;
      n_connected_clients += 1;

      pthread_cond_signal(&new_player_cond);
      pthread_mutex_unlock(&game_mutex);
    } else {
      /* unkown client sent something unexpected */
      printf("Unknown client sent a message to the server but did not request to play\n");
    }
  }

  else if(client_id <= 1){
    /* assigned player sent a message */
    game_message g_msg;
    g_msg.player_id = client_id;
    parse_data(info->buffer, &g_msg);

    if(g_msg.code == MOV && n_connected_clients == 2){
      /* the player made a move */
      /* tell the game loop thread that a new move was registered */

      pthread_mutex_lock(&game_mutex);
      last_move = g_msg;

#if DEBUG_MODE
      printf("+-----------------------------+\n");
      printf("Move Received: player %d\n", last_move.player_id);
      printf("Row, Col = (%d, %d)\n", last_move.data[1], last_move.data[0]);
#endif
      if (last_move.player_id != game.player_to_move){
        /* player tried to move when it was not his/her turn */
        send_txt(players[last_move.player_id], "Your move was ignored. It is not your turn.");
#if DEBUG_MODE
      printf("Move was ignored.\n");
#endif
      } else {
        pthread_cond_signal(&new_move_cond);
      }
      pthread_mutex_unlock(&game_mutex);
    } else {
      /* client sent a message that was unexpected */
      send_txt(info->client_addr, "Your message was not expected and thus will be ignored.");
    }
  }
  
  free(info);
  return NULL;
}

/**
 * 
 * indentify_client -
 * Determines if a client was already assigned or not. 
 * 
 * Returns 0 if client is the player 1
 * Returns 1 if client is the player 2
 * Returns 2 if client is not assigned
 * 
 */
int identify_client(const struct sockaddr_in *addr){

  int i;
  for(i=0; i<MAX_CLIENTS; ++i){
    if (!memcmp(addr, &players[i], sizeof(struct sockaddr_in))){
      return i;
    }
  }

  return 2;
}

/**
 * 
 * parse_data - 
 * Transforms raw bytes into a game_message object that
 * contains the same data.
 */
int parse_data(char *data, game_message *g_msg){
  /* finds the type of the message */
  g_msg->code = data[0];
  if (g_msg->code != MOV && g_msg->code != TXT) {
    fprintf(stderr, "Type of Message not recognized\n");
    return 1;
  }

  memcpy(g_msg->data, data+1, MAX_SIZE-1);
  return 0;
}

/**
 * 
 * game_loop - 
 * Loop responsible for the logic of the game. This thread
 * keeps waiting for players to join. When there are exactly 
 * 2 players, it waits for each player to make its move, and 
 * then sends the appropriate messages to both players.
 * 
 */
void *game_loop(void *params){

  pthread_mutex_lock(&game_mutex);
  while(1){
    
    /* restarts the board */
    initialize_game();

    while(n_connected_clients < 2){
      /* do nothing */
      pthread_cond_wait(&new_player_cond, &game_mutex);
    }

    /* sends the FYI message with an empty 3x3 grid */
    send_information_messages();

    while(!game.is_game_over){

      while(last_move.player_id != game.player_to_move || last_move.processed){
        /* wait for message from handler */
        
        /* asks for the client to send his/her move */
        udp_info info;
        info.buffer[0] = MYM;
        info.n_bytes = 1;
        info.client_addr = players[game.player_to_move];
        info.len = sizeof(struct sockaddr_in);
        send_data(&info);

        /* waits for a new move to come */
        pthread_cond_wait(&new_move_cond, &game_mutex);
      }

      int col = (int)last_move.data[0];
      int row = (int)last_move.data[1];

      /* checks if the move is valid */
      /* in case the move is not valid, it asks for the client to send a new move */
      if (row < 0 || row > 2 || col < 0 || col > 2) {
        printf("Player %d tried to make illegal move\n", last_move.player_id);
        send_txt(players[last_move.player_id], "Invalid Move: position is not in the grid");

      } else if (game.cells[row][col]) {
        printf("Player %d tried to make illegal move\n", last_move.player_id);
        send_txt(players[last_move.player_id], "Invalid Move: position is already taken");
      } else {
        /* move is valid */
        game.cells[row][col] = (char) (last_move.player_id + 1);
        game.player_to_move = 1 - last_move.player_id;
        game.n_occupied += 1;

        /* send the FYI message with the new updated board */
        send_information_messages();

        /* checks now if game is over */
        update_game_status();
      }

      last_move.processed = 1;
    }

    /* sends the results to both players */
    /* resets the players so that now new players can join */
    finalize_game();
  }    
}

/**
 * 
 * update_game_status - 
 * Checks if the game is over. In case it is over, it sets
 * the outcome of the game in the game global variable.
 * 
 */
void update_game_status(void){

  int i=0;

  /* check rows */
  for(i=0; i<3; ++i){
    if (game.cells[i][0] == game.cells[i][1] && game.cells[i][1] == game.cells[i][2] &&
      game.cells[i][2] != 0) {
        game.is_game_over = 1;
        game.game_result = game.cells[i][0];
        return;
    }
  }

  /* check cols */
  for(i=0; i<3; ++i){
    if (game.cells[0][i] == game.cells[1][i] && game.cells[1][i] == game.cells[2][i] &&
      game.cells[2][i] != 0) {
        game.is_game_over = 1;
        game.game_result = game.cells[0][i];
        return;
    }
  }

  /* check diagonals */
  if (game.cells[0][0] == game.cells[1][1] && game.cells[1][1] == game.cells[2][2] &&
    game.cells[2][2] != 0){
      game.is_game_over = 1;
      game.game_result = game.cells[0][0];
      return;
  }

  if (game.cells[0][2] == game.cells[1][1] && game.cells[1][1] == game.cells[2][0] &&
    game.cells[2][0] != 0){
      game.is_game_over = 1;
      game.game_result = game.cells[0][2];
      return;
  }

  /* checks if the game was a draw */
  if (game.n_occupied == 9) {
    game.is_game_over = 1;
    game.game_result = 0;
  }

}

/**
 * 
 * initialize_game - 
 * Resets the board so that a new game can start.
 * 
 */
void *initialize_game(void){
  printf("+-----------------------------+\n");
  printf("Creating a new game.\n");

  last_move.player_id = 2;
  last_move.processed = 1;

  game.n_occupied = 0;
  game.player_to_move = 0;
  game.is_game_over = 0;
  memset(game.cells, 0, 3*3*sizeof(char));

  return NULL;
}

void *finalize_game(void){

  printf("+-----------------------------+\n");
  printf("Game is over.\n");
  printf("Player %d won.\n", game.game_result);

  int i;
  for(i=0; i<MAX_CLIENTS; ++i){
    udp_info info;
    
    info.client_addr = players[i];
    info.len = sizeof(struct sockaddr_in);

    info.buffer[0] = END;
    info.buffer[1] = game.game_result;
    info.n_bytes = 2;

    send_data(&info);
    memset(&players[i], 0, sizeof(players[i]));
  }

  n_connected_clients = 0;
  return NULL;
}

/**
 * 
 * send_information_messages - 
 * Sends the FYI messages to both players.
 */
void *send_information_messages(void){

  /* sends FYI messages */
  int i;
  for(i=0; i<MAX_CLIENTS; ++i){
    udp_info info;
    info.client_addr = players[i];
    info.len = sizeof(struct sockaddr_in);

    info.buffer[0] = FYI;
    info.buffer[1] = (char) game.n_occupied;

    int idx = 2;
    int col, row;

    for(row=0; row<3; ++row){
      for(col=0; col<3; ++col){
        if(game.cells[row][col]){
          info.buffer[idx] = game.cells[row][col];
          info.buffer[idx+1] = (char) col;
          info.buffer[idx+2] = (char) row;
          idx += 3;
        }
      }
    }

    info.n_bytes = idx;

    send_data(&info);

  }

  return NULL;
}

/**
 *
 * send_data - 
 * The function rensponsible for sending messages to the clients.
 * It takes the udp_info object that contains all the information necessary
 * for the contact to occur.
 *
 */
void *send_data(udp_info *info){

#if DEBUG_MODE
  /* found here the instructions to print IP address */
  // https://stackoverflow.com/questions/9590529/how-should-i-print-server-address
  char buffer[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &info->client_addr.sin_addr, buffer, INET_ADDRSTRLEN);

  printf("+-----------------------------+\n");
  printf("Sending Data to: %s::%d\n", buffer, htons(info->client_addr.sin_port));
  print_bytes((void *) info->buffer, info->n_bytes);
#endif
  if (sendto(sockfd, (const char *)info->buffer, info->n_bytes,
             MSG_CONFIRM, (const struct sockaddr *)&info->client_addr, info->len) < 0){

    perror("sendto");
  }

  return NULL;
}

/**
 * 
 * send_txt - 
 * Sends a specific string to a specific address through
 * udp.
 * 
 */
void send_txt(struct sockaddr_in addr, char *message){

  udp_info info;
  info.client_addr = addr;
  info.len = sizeof(struct sockaddr_in);

  info.buffer[0] = TXT;

  strncpy(info.buffer + 1, message, MAX_SIZE - 2);
  info.n_bytes = strlen(info.buffer + 1) + 2;
  info.buffer[info.n_bytes - 1] = '\0';

  send_data(&info);
}

/* debug function */
/**
 * 
 * print_bytes - 
 * Prints the next len bytes in memory after
 * the pointer ptr.
 */
void print_bytes(void *ptr, int len) {

  printf("[%d bytes]\n", len);
  
  int i=0;
  for(i=0; i<len; ++i){
    printf("%x ", *(char *)(ptr + i));
  }

  printf("\n");
}