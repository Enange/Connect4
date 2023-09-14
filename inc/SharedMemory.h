#ifndef _SHAREDMEMORY_HH
#define _SHAREDMEMORY_HH

// struttura della memoria condivisa
struct shmseg
{
    char gettoni[2];
    int counter;
    int player;
    int righe;
    int colonne;
    int draw;
    int checkWin;
    int SP;
    int bot;

    /** 
     * ! ALTERNATIVA PER I PID
     * usare la struttura data dalla shared Memory, SHMID_DS
    */
   
    pid_t server;
    pid_t client[2];
};

#endif