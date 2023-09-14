#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include<stdbool.h>

#include<sys/shm.h>
#include<sys/sem.h>
#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/stat.h>
#include<signal.h>


#include "Semaphore.h"
#include "errExit.h"
#include "SharedMemory.h"

#define KEY1 89
#define KEY2 90
#define KEYS 91


struct shmseg* memCondivisa;
char* CampoCondiviso;
char Gettone;
int Alarm;
int botIsPlaying = 0;
int pressCtrlc = 0;

//FUNZIONI E PROCEDURE
void printMat();
int isNotValid(int);
void move(int, char);
void WL(int signal);
void Ctrlc(int signal);
void PlayerLeft(int signal);
void ServerLeft(int signal);
void Timeout(int signal);
void SwapPlayer();

int main(int argc, char const *argv[])
{
    int scelta;

    /***********************
    /  GESTISCO I SEGNALI
    ************************/

    if(signal(SIGUSR1, WL) == SIG_ERR){
        errExit("Segnale Vittoria/Sconfitta Fallito:");
    }     // Segnale che gestisce vincità e perdità
    if(signal(SIGINT, Ctrlc) == SIG_ERR){
        errExit("Segnale CTRL+c");
    }
    if(signal(SIGUSR2, PlayerLeft) == SIG_ERR){
        errExit("Segnale CTRL+c");
    }
    if(signal(SIGTERM, ServerLeft) == SIG_ERR){
        errExit("Segnale CTRL+c");
    }
    if(signal(SIGALRM, Timeout) == SIG_ERR){
        errExit("Segnale Timeout");
    }

    /***************************************
    / ATTACCO SEMAFORI e MEMORIA CONDIVISA
    ***************************************/


    int shmid1 = shmget(KEY1,0,0);
    if(shmid1 == -1){
        errExit("Apertura memoria");
    }

    //attacco memoria
    CampoCondiviso = (char*)shmat(shmid1 ,NULL ,0);
    if(CampoCondiviso == (void*)-1){
        errExit("Attach: ");
    }

    // 
    
    int shmid2 = shmget(KEY2,0,0);
    if(shmid2 == -1){
        errExit("Apertura memoria");
    }

    //attacco memoria
    memCondivisa = (struct shmseg*)shmat(shmid2 ,NULL ,0);
    if(memCondivisa == (void*)-1){
        errExit("Attach: ");
    }

    // semafori
    int semid = semget(KEYS, 0, 0);
    if(semid == -1){
        errExit("Semafori: ");
    }

    semOp(semid,(unsigned short)3,-1); //P(mutex)

    /***********************
    /  GESTISCO IL SINGLE PLAYER
    ************************/
    if(argc > 2){
        if(argc > 3){
            printf("Errore input. Se vuoi utilizzare la modalità giocatore singolo usa '*'\n");
            semOp(semid,(unsigned short)3,1); //V(mutex)
            return 0;
        }
        if(argv[2][0] == '*'){
            memCondivisa->SP = 1;
            semOp(semid, (unsigned short) 2, 1);        //V(server)
        }
    }
    
    

    // Prendo il mio simbolo
    Gettone = memCondivisa->gettoni[memCondivisa->counter];
    // Do i gettoni
    memCondivisa->client[memCondivisa->counter]= getpid();
    
    

    //Controllo il Client (se sono il primo giocatore oppure il secondo)
    memCondivisa->counter++;
    // GESTISCO IL RANDOM SP


    if(memCondivisa->counter != 2){
        semOp(semid, (unsigned short)3, 1);             //V(mutex)

        // Stampo il nome e simbolo
        printf("'%s' connesso con il simbolo '%c'\nPremere [CTrl+c] per abbandonare\n",argv[1],Gettone);
        // Notifico che sono in attesa del secondo giocatore
        printf("In attesa di un altro giocatore...\n");
        //fflush(stdout);

        semOp(semid, (unsigned short)0,-1);             //P(client1)
        if(memCondivisa->client[1] == 0){
            semOp(semid, (unsigned short)0,-1);
        }
        printf("Giocatore Avversario connesso\n");     
    }
    else{
       
        // stampo nome e simbolo associato
        if(memCondivisa->bot == 1){
            botIsPlaying = 1;
        }
        else{
            printf("'%s' connesso con il simbolo '%c'\nPremere [CTrl+c] per abbandonare\n",argv[1],Gettone);
            // Sono in attesa
            printf("In attesa di Avversario\n"); //, memCondivisa->nome[0]);
        }
        semOp(semid, (unsigned short)3, 1);             //P(mutex)
        semOp(semid, (unsigned short)0, 1);             //V(client1)
        semOp(semid, (unsigned short)1, -1);            //P(Client2)
        if(pressCtrlc ){
            pressCtrlc = 0;
            semOp(semid, (unsigned short) 1, -1);       //P(client2)
        }
    }
    // verra fermato dal segnale



    while(1){

        alarm(15);
    
        if(botIsPlaying){
            do{
                scelta = rand() % (memCondivisa->colonne);
            }while(isNotValid(scelta));
        }
        else{
            // stampo matrice
            printMat();

            // Verifico la scelta
            do{
                printf("\n\nColonna da inserire:");
                
                scanf("%d", &scelta);

               /* if(!scanf("%d", &scelta)){
                    scanf("%*[^\n]");                   // prende tutte le stringhe fino alla fine della nuova linea
                    scelta = -1;
                }
*/

            }while(isNotValid(scelta));
        }
        

        alarm(0);                       // Annula il segnale
       
        // faccio la mossa
        move(scelta, Gettone);

        // Cambio il Player
        SwapPlayer();
    }



    return 0;
}


void printMat(){

    printf("\n");
    for(int i = 0; i<memCondivisa->righe; i++){
        printf("%5c", '|');
        for(int j = 0; j<memCondivisa->colonne; j++){
            printf(" %c |",CampoCondiviso[i*memCondivisa->colonne + j]);
       }
       printf("\n");
    }

    // Linea orizzontale e numeri
    printf("%4c%c", ' ','=');
    for(int j = 0; j<memCondivisa->colonne; j++){
        printf("====");
    }
    printf("\n");
    printf("%4c", ' ');
    for(int j = 0; j<memCondivisa->colonne; j++){
        if(j == 10){
            printf(" ");
        }
    
        printf(" %2d ", j);
    
        
    }

/* CONTROLLO COLONNE PIENE
    for(int i = memCondivisa->colonne*memCondivisa->righe; i<memCondivisa->colonne*memCondivisa->righe+memCondivisa->colonne;i++){
        printf("\n%d\n",CampoCondiviso[i]-48);
    }

    */
}

int isNotValid(int scelta){

    int colonnaScelta = memCondivisa->colonne*memCondivisa->righe+scelta;
    if(scelta > memCondivisa->colonne || scelta < 0 || CampoCondiviso[colonnaScelta]-1 < '0'){
        if(!botIsPlaying){
            printf("Colonna non valida");
        }
        return 1;
    }
    
    CampoCondiviso[colonnaScelta]-=1;
    return 0;
}


void move(int scelta, char Gettone ){

    int colonnaScelta = memCondivisa->colonne*memCondivisa->righe+scelta;
    CampoCondiviso[(CampoCondiviso[colonnaScelta]-48)*memCondivisa->colonne+scelta] = Gettone;
}

void SwapPlayer(){

    int semid = semget(KEYS,0,0);
    semOp(semid,(unsigned short) 3, -1);        // P(Mutex)

    if(memCondivisa->player == 0){
        memCondivisa->player = 1;   
        semOp(semid, (unsigned short) 3, 1);        //V(mutex)
        semOp(semid, (unsigned short) 2, 1);        //V(server)
        printf("In attesa dell'avversario...\n");
        fflush(stdout);
        semOp(semid, (unsigned short) 0, -1);       //P(client1)
    } 
    else{
        memCondivisa->player = 0;
        semOp(semid, (unsigned short) 3, 1);        //V(mutex)
        semOp(semid, (unsigned short) 2, 1);        //V(server)
        if(!botIsPlaying){
            printf("In attesa dell'avversario...\n");
            fflush(stdout);
        }
        semOp(semid, (unsigned short) 1, -1);       //P(client2)
        if(pressCtrlc ){
            pressCtrlc = 0;
            semOp(semid, (unsigned short) 1, -1);       //P(client2)
        }
    }

    if(Alarm){
        Alarm = 0;
        alarm(15);
        printMat();
        printf("\n Colonna da inserire:");
        fflush(stdout);
    }
    
    
}

/*
1 2 3 
O 5 6
X 8 9

0 1 2 


0='1'
1=3
2=3

1 * Colonne + scelta*/

void WL(int signal){

    if(!botIsPlaying){
        if(memCondivisa->draw == 1){
            printMat();
            printf("\n\nPAREGGIO !!!");
            exit(0);
        }
        else{
            if(Gettone == memCondivisa->gettoni[(memCondivisa->player+1)%2]){
                
                printMat();
                printf("\n\nHAI VINTO !!!\n");

                exit(0);
            }
            else{
                
                printMat();
                printf("\n\nHAI PERSO !!!\n");

                exit(0);
            }
        }
    }
    exit(0);
      
}

void Ctrlc(int signal){
    if(!botIsPlaying){
    memCondivisa->player = (Gettone == memCondivisa->gettoni[0])? 1 : 0;    // Quale player vincerà
    kill(memCondivisa->server, SIGUSR2);
    // mi uccido
    printf("\nPerdita per abbandono\n");
    exit(0);
    }else{
        pressCtrlc = 1;
    }
}
void PlayerLeft(int signal){
    if(!botIsPlaying)
        printf("\nL'altro giocatore ha abbandonato \n\nHAI VINTO A TAVOLINO\n");
    exit(0);
}
void ServerLeft(int signal){
    printf("\nIl server ha interrotto la partita\n");
    exit(0);
}
void Timeout(int signal){
    
    Alarm = 1;
    printf("\nTempo Scaduto\n");

    SwapPlayer();
}
   