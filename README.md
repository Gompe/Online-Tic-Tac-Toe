# Tic-Tac-Toe
## How to use

### Compilation:

`$ make`

`$ make clean`

### Use 

#### Server

`$ ./server PORT`

This will start a server at the specified PORT number.

There is no need to give any user input to the server. The server, however, will print some informative messages to the terminal.

Whenever a client connects, the server will send either message:

(1) [TXT] And will welcome the client and specify what the client will play with (X or O).
(2) [END] If there are already to clients in the server, the server will answer any further connection attempts with an [END] 0xff message. 

After 2 clients connect, the game will start. After each move, the server will send the board information to both clients with a message of the kind [FYI].
Then, it will ask the correct player to move with a message of the kind [MYM].

If a client tries to send any message to the sever when its not its turn, the message will simply be ignored.

When the game is over, it will send the outcome to both players with a message of the kind [END]. Moreover, it will start accepting connections again until
2 more clients connect and a new game starts.

#### Client

`$ ./client IP_ADDRESS PORT $`

Connects to a server in the specified (IP_ADDRESS, PORT) location. To establish connection, send the following through the terminal:

`$ TXT Hello `

Any other string other than "Hello" will not cause connection.

After two clients connect, the game will start and there will be the following message in terminal when you are required to perform a move:

[MYM]

To make a move at a specific row and a specific column, write 

`$ MOV col row`

For example, 

`$ MOV 1 2`

When the game is over, the outcome will be printed to the terminal and the program will finish its execution.
