#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>


#include<sys/shm.h>
#include<sys/sem.h>
#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/stat.h>
#include<signal.h>
#include<sys/wait.h>

#include "Semaphore.h"
#include "errExit.h"
#include "SharedMemory.h"

#define KEY1 89
#define KEY2 90
#define KEYS 91

struct shmseg* memCondivisa;
char* CampoCondiviso;
int pressCtrlc = 0;


void checkWinner();
char checkSymbol(int,int);
int checkDraw();
void Ctrlc(int signal);
void PlayerLeft(int signal);
void closeAll();

int main(int argc, char const *argv[]){

    /*Controllo se i parametri in entrata sono validi, altrimenti Spiego e termino*/
    if(argc !=  5 ){
        printf("./command <num righe> <num colonne> <SimboloClient1> <SimboloClient2>\n");
        return 0;
    }  

    // VARIABILI
    int FinePartita = 1;
    int size = atoi(argv[1])*atoi(argv[2]);             // Dimensione campo condiviso

  
    
    /*Controllo singoli parametri*/

    if(atoi(argv[1]) < 5 || atoi(argv[2]) < 5){             // controllo num. righe e colonne
        printf("Il campo di gioco e' di 5x5 minimo\n");
        return 0;
    }

    if(strlen(argv[3])!= 1 || strlen(argv[4])!= 1 || argv[3][0] == argv[4][0] ){
        printf("Le forme dei gettoni devono essere diverse e formate da un solo carattere\n");  // controllo gettoni
        return 0;
    }

    // GESTIONE SEGNALI
    
    if(signal(SIGINT, Ctrlc) == SIG_ERR){
        errExit("Segnale CTRL+c");
    }
    if(signal(SIGUSR2, PlayerLeft) == SIG_ERR){
        errExit("Segnale Player Left the game");
    }



    /***************************************
    / Creazione / Gestione MEMORIA CONDIVISA 
    ***************************************/
           
    int shmid2 = shmget(KEY2, sizeof(struct shmseg), IPC_CREAT | IPC_EXCL | 0660);

    // ERRORE Memoria già esistente -> PULISCO
    if(shmid2 == -1){
        //printf("Elimino gia esistente");
        if(shmctl(shmget(KEY2,0,0),IPC_RMID, NULL) == -1)
            errExit("Eliminazione Memoria: ");

        // Ricreo la memoria
        shmid2 = shmget(KEY2, sizeof(struct shmseg), IPC_CREAT | IPC_EXCL | 0660);   // Permission:  - (rw-) (rw-) (---)            r=4 w=2 x=1 -=0

        // Controllo se nel secondo tentativo ho creato la memoria
        if(shmid2 == -1){
            errExit("Creazione Memoria: ");
        }
    }

    // ATTACCO MEMORIA
    memCondivisa = (struct shmseg*)shmat(shmid2,NULL,0);

    if(memCondivisa == (void*)-1){
        errExit("Errore Attach: ");
    }
    


    // MEMORIA CAMPO
           
    int shmid1 = shmget(KEY1, sizeof(char)*(size+atoi(argv[2])), IPC_CREAT | IPC_EXCL | 0660);

    // ERRORE Memoria già esistente -> PULISCO
    if(shmid1 == -1){
        //printf("Elimino gia esistente");
        if(shmctl(shmget(KEY1,0,0),IPC_RMID, NULL) == -1)
            errExit("Eliminazione Memoria: ");

        // Ricreo la memoria
        shmid1 = shmget(KEY1, sizeof(char)*(size+atoi(argv[2])), IPC_CREAT | IPC_EXCL | 0660);   // Permission:  - (rw-) (rw-) (---)            r=4 w=2 x=1 -=0

        // Controllo se nel secondo tentativo ho creato la memoria
        if(shmid1 == -1){
            errExit("Creazione Memoria: ");
        }
    }

    // ATTACCO MEMORIA
    CampoCondiviso = (char*)shmat(shmid1,NULL,0);

    if(CampoCondiviso == (void*)-1){
        errExit("Errore Attach: ");
    }

    
    // RIEMPIO IL CAMPO
    for(int i = 0; i<size; i++){
        CampoCondiviso[i] = ' ';
    }
    // riempio il counter
    for(int i = size; i< size+atoi(argv[2]); i++){
        CampoCondiviso[i] = (char) atoi(argv[1])+48;  
    }
   
    // RIEMPIO LA Struct shmseg memCondivisa
    memCondivisa->gettoni[0] = argv[3][0];       // gettone giocatore 1 
    memCondivisa->gettoni[1] = argv[4][0];       // gettone giocarere 2
    memCondivisa->counter = 0;
    memCondivisa->player = 0;
    memCondivisa->righe = atoi(argv[1]);
    memCondivisa->colonne = atoi(argv[2]);
    memCondivisa->server = getpid();
    memCondivisa->draw = 0;
    memCondivisa->checkWin = 1;
    

    /*Creazione SEMAFORI*/

    int semid = semget(KEYS, 4, IPC_CREAT | IPC_EXCL |S_IRUSR | S_IWUSR);

    if(semid ==-1){
        // Elimino semafori gia esistenti
        if(semctl(semget(KEYS,0,0), 0, IPC_RMID, 0) == -1){
            errExit("semctl IPC_RMID: ");
        }
        // ricreo i semafori
        semid = semget(KEYS, 4, IPC_CREAT | IPC_EXCL |S_IRUSR | S_IWUSR);
        // POssibile errore
        if(semid == -1){
            errExit("Crezione Semafori: ");
        }
    }

    // inizializzazione semaf
    // Ordine semafori: Client1, Client2, Server, Mutex
    unsigned short semVal[]= {0,0,0,1};
    union semun arg;
    arg.array = semVal;

    // Setto i semafori
    if(semctl(semid, 0, SETALL, arg) == -1){
        errExit("errore Inizializzazione Semafori: ");
    }

    semOp(semid,(unsigned short) 2,-1); // P(Server)
    semOp(semid,(unsigned short) 3,-1); // P(Mutex)


    // Controllo Giocatore Singolo
    if(memCondivisa->SP == 1){

        // FIGLIO CLIENT
        int pid = fork();
        if(pid == -1){
            errExit("Errore figlio: ");
        }
        else if(pid == 0){
            // Nascita del figlio client che gioca in SP
            memCondivisa->bot = 1;
            char *args[] = {"bot", NULL};
            execv("./F4Client", args);
            
        }

       
        semOp(semid,(unsigned short) 3,1); // V(mutex)

    }
    else{
        semOp(semid,(unsigned short) 2,1); // V(Server)
        semOp(semid,(unsigned short) 3,1); // V(mutex)
    }



    while(FinePartita){
        // aspetto l'arrivo dei client
        semOp(semid,(unsigned short) 2,-1); // P(Server)
        if(pressCtrlc){
            pressCtrlc = 0;
            semOp(semid,(unsigned short) 2,-1); // P(Server)
        }

        // CONTROLLO
        //STATO DELLA PARTITA
        checkWinner();
        if(memCondivisa->checkWin == 1){
            kill(memCondivisa->client[0], SIGUSR1);
            kill(memCondivisa->client[1], SIGUSR1);
            
            
            printf("Partita finita con vincitore\n");
            fflush(stdout);

            // Chiudo il server 
            closeAll();
            exit(0);

        }else if (checkDraw()){
            kill(memCondivisa->client[0], SIGUSR1);
            kill(memCondivisa->client[1], SIGUSR1);

            printf("Partita finita in pareggio !\n");
            fflush(stdout);


            // Chiudo il server 
            closeAll();
            exit(0);


            
        }

        semOp(semid, (unsigned short) 3, -1);        //P(mutex)

        if(memCondivisa->player == 0){                  // Turno giocatore 1
            semOp(semid, (unsigned short) 3, 1);        //V(mutex)
            semOp(semid, (unsigned short) 0, 1);        //V(client1)
        }
        else{                                           /// Turno giocatore 2
            semOp(semid, (unsigned short) 3, 1);        //V(mutex)
            semOp(semid, (unsigned short) 1, 1);        //V(client2)

        }
    }  

    return 0;
}


void checkWinner(){

    

    // CONTROLLO DIAGONALE gestito dal figlio
    memCondivisa->checkWin = 0;
    int status;
    int pid=fork();
    
    if(pid == -1){
        errExit("fork");
    }
    else if(pid == 0){
        

        char gettone = memCondivisa->gettoni[(memCondivisa->player+1)%2];         // prendo il gettone giusto 
        int count = 0;
        // CONTROLLO ORIZZONTALE
        for(int i = 0; i< memCondivisa->righe; i++){
            count = 0;
            for(int j = 0; j<memCondivisa->colonne; j++){
                if(checkSymbol(i,j) == gettone){
                    count++;  
                }
                else{
                    count =0;
                }
                if(count == 4){
                    memCondivisa->checkWin = 1;
                }
            }
        }
        
        // CONTROLLO VERTICALE
        for(int i = 0; i< memCondivisa->colonne && memCondivisa->checkWin == 0; i++){
            count = 0;
            for(int j = 0; j<memCondivisa->righe; j++){
    
                if(checkSymbol(j,i) == gettone){
                    count++;
                }
                else{
                    count =0;
                }
                if(count == 4){
                    memCondivisa->checkWin = 1;
                }
            }   
        }
        // DIAGONALE PRICIPALE  -> 
        for(int i = 0; i<memCondivisa->righe-3 && memCondivisa->checkWin == 0; i++){
            for(int j = 0; j< memCondivisa->colonne-3; j++){
                if( gettone == checkSymbol(i,j)     && 
                    gettone == checkSymbol(i+1,j+1) &&
                    gettone == checkSymbol(i+2,j+2) &&
                    gettone == checkSymbol(i+3,j+3))
                    memCondivisa->checkWin = 1;
            }
        }

        // DIAGONALE SECONDARIA
        for(int i = 0; i<memCondivisa->righe-3 && memCondivisa->checkWin == 0; i++){
            for(int j = 3; j<memCondivisa->colonne; j++){
                if( gettone == checkSymbol(i,j)     && 
                    gettone == checkSymbol(i+1,j-1) &&
                    gettone == checkSymbol(i+2,j-2) &&
                    gettone == checkSymbol(i+3,j-3))
                    memCondivisa->checkWin = 1;
                    
            }
        
        }
        exit(0);
    }
    else{
        // padre aspetta il figlio
        waitpid(-1, &status, 0);
       
    }

}

char checkSymbol(int i, int j){
    return CampoCondiviso[i*memCondivisa->colonne + j];
}

int checkDraw(){
    for(int i = memCondivisa->colonne*memCondivisa->righe; i<memCondivisa->colonne*memCondivisa->righe+memCondivisa->colonne;i++)
        if(CampoCondiviso[i]-48 != 0){
            
            return 0;
        }

    memCondivisa->draw= 1;
    return 1;
}

void Ctrlc(int signal){

    // ? FACCIAMO A TEMPO
    static int count = 0;
    
    count++;
    
    if(count == 2){
        if(memCondivisa->client[0] != 0){
            // Gestisco il caso in cui il server viene chiuso prima del 
            // join di un client
            kill(memCondivisa->client[0], SIGTERM);
        }
        if(memCondivisa->client[1] != 0){
            // Gestisco il caso in cui il server interrompe prima del join
            // del secondo client
            kill(memCondivisa->client[1], SIGTERM); 
        }
        closeAll();
        exit(0);
    }
    printf("\nè stato premuto %d volta 'Ctrl+c', alla seconda verrà interrotto il gioco\n", count);
    fflush(stdout);
    pressCtrlc = 1;

}

void PlayerLeft(int signal){
    kill(memCondivisa->client[memCondivisa->player], SIGUSR2);
    closeAll();
    exit(0);
}

void closeAll(){

    if(shmctl(shmget(KEY1,0,0),IPC_RMID, NULL) == -1){
        errExit("shmct IPC_RMID");
    }
    if(shmctl(shmget(KEY2,0,0),IPC_RMID, NULL) == -1){
        errExit("shmct IPC_RMID");
    }
    if(semctl(semget(KEYS,0,0), 0, IPC_RMID, 0) == -1){
        errExit("semctl IPC_RMID: ");
    }
}
