# Connect4

## About the game: 
The game is played between two players on a rectangular field of at least 5x5 dimensions, where each player takes turns dropping their token, which will settle at the bottom (or on top of another token already in that column). The player who wins will be the first player to successfully align 4 of their tokens (horizontally, vertically, diagonally) without interruption. The game is programmed in C and involves the use of system calls.

## How to play: 
Execute the following commands in the bash shell:
''' console

make F4Server

'''
After this, open two more terminal windows, in the first one start the server.
''' console

./F4Server X 0 5 5 

'''
Where X and O are the tokens assigned to the players, and the two following numbers indicate the field's dimensions. The field size has no limits, but it is recommended to play in a maximum of 10x10.
Now, in the other terminal, start the first client with the nickname of your choice using the command:
''' console

./F4Client David 

'''
Now, this player will remain waiting for another player to join. Perform the same operation in the other terminal using the same command.
The game is ready! Have fun!
