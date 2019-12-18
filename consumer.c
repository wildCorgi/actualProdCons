#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/shm.h>
#include <time.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define sharedMemory 999
#define mutexKey 111
#define emptyKey 222
#define fullKey 3333
#define userVal 4444
#define prod 5555
#define userValType 6666
#define bufinfoType 7777
#define cons 9999
#define prodrcv 3100

int shmid;
int producerQueue;
int consumerQueue;
union Semun {
    int val;               /* value for SETVAL */
    struct semid_ds *buf;  /* buffer for IPC_STAT & IPC_SET */
    unsigned short *array; /* array for GETALL & SETALL */
    struct seminfo *__buf; /* buffer for IPC_INFO */
    void *__pad;
};
union Semun semun;

struct msgbuff
{
    long mtype;
    int userValue;
};

void down(int sem)
{
    struct sembuf p_op;

    p_op.sem_num = 0;
    p_op.sem_op = -1;
    p_op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &p_op, 1) == -1)
    {
        perror("Error in down()");
        exit(-1);
    }
}

void up(int sem)
{
    struct sembuf v_op;

    v_op.sem_num = 0;
    v_op.sem_op = 1;
    v_op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &v_op, 1) == -1)
    {
        perror("Error in up()");
        exit(-1);
    }
}

int consumeItem(int *shm)
{

    int lastIndex = shm[1] - 1;
    int consumable = shm[lastIndex];
    shm[1] = shm[1] - 1;
    return consumable;
}

void consumer(int *shm, int mutexSem, int fullSem, int emptySem)
{
    while (1)
    {
        //int semvalue = semctl(fullSem, 0, GETVAL, semun);
        
        if (semctl(fullSem, 0, GETVAL, semun) == 0)
        {
            perror("fe error");
            write(1, "The Buffer is empty, consumer sleeps \n", 39);
            struct msgbuff result;
            //sleep(3);
            char msg[50] = "I am blocking in the consumer message receive. \n";
            write(1,msg, sizeof(msg));
            int MSGRCV = msgrcv(producerQueue, &result, sizeof(result.userValue), prodrcv, !IPC_NOWAIT);
            if (MSGRCV == -1)
            {
                perror("error receiving wakeup call from producer. \n");
            }
            else
            {
                write(1, "Consumer has been awaken. \n", 27);
            }
        }
        
        down(fullSem);  //down full
        down(mutexSem); // down mutex
        printf("emptysem is %d \n",semctl(emptySem, 0, GETVAL, semun));
        if (semctl(emptySem, 0, GETVAL, semun) == 0)
        {
            int lastIndex = shm[1];
            int consumerism = consumeItem(shm);
            printf("consumed %d, at %d \n", consumerism, lastIndex);
            struct msgbuff tobeSent;
            tobeSent.mtype = prodrcv;
            tobeSent.userValue = 987;
            
            int MSGSND = msgsnd(consumerQueue, &tobeSent, sizeof(tobeSent.userValue), !IPC_NOWAIT);
            if (MSGSND == -1)
            {
                perror("couldn't send wakeup message to producer. \n");
            }
            else
            {
                printf("sent a wakeup message to the producer \n");
            }
        }
        else
        {
            int lastIndex = shm[1];
            int consumerism = consumeItem(shm);
            printf("normal consumed %d at %d \n", consumerism, lastIndex);
        }
       
        up(mutexSem); // up mutex
       
        up(emptySem); //up empty
       
    }
}

/* arg for semctl system calls. */

int main()
{
    //int userValQueue = msgget(userVal, IPC_CREAT | 0666); //userValQueue to receive usersize from producer
    producerQueue = msgget(prod, IPC_CREAT | 0666); //producerQueue to receive wakeup calls from producer
    consumerQueue = msgget(cons, IPC_CREAT | 0666); //consumerQueue to send wakeup calls to producer

    //receiving user Size from producer
    struct msgbuff mymessage;
    //int MSGRCV = msgrcv(userValQueue, &mymessage, sizeof(mymessage.userValue), userValType, !IPC_NOWAIT);

    //retrieving shared memory that contains the buffer
    shmid = shmget(sharedMemory, sizeof(mymessage.userValue), IPC_CREAT | 0644);
    int *shm = (int *)shmat(shmid, 0, 0);

    //retrieving semaphores
    int mutexSem = semget(mutexKey, 1, 0666 | IPC_CREAT);
    int emptySem = semget(emptyKey, 1, 0666 | IPC_CREAT);
    int fullSem = semget(fullKey, 1, 0666 | IPC_CREAT);
    if (mutexSem == -1 || emptySem == -1 || fullSem == -1)
    {
        perror("Error in creating Semaphore");
        exit(-1);
    }

    //calling the consumer
    consumer(shm, mutexSem, fullSem, emptySem);

    return 0;
}
