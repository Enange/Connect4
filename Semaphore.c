#include <sys/sem.h>
#include <stdio.h>
#include<errno.h>

#include "Semaphore.h"
#include "errExit.h"

void semOp (int semid, unsigned short sem_num, short sem_op) {
    struct sembuf sop = {.sem_num = sem_num, .sem_op = sem_op, .sem_flg = 0};

    if (semop(semid, &sop, 1) == -1){
        int errsv = errno;
        if(errsv != EINTR ){
            errExit("semop: ");    
        }
    }
        
}