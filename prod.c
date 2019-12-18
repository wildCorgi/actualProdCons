#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/shm.h>
#include <string.h>
#include <time.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <signal.h>
#include <sys/msg.h>

#define sharedMemory 999
#define mutexKey 111
#define emptyKey 222
#define fullKey 3333
#define userVal 4444
#define prod 5555
#define cons 9999
#define userValType 6666
#define bufinfoType 7777
#define prodrcv 3100

int shmid;         //shared memory size
int userSize;      //user size for the buffer
int producerQueue; //the queue the producer sends on
int consumerQueue; //the queue the consumer sends on

struct msgbuff
{
    long mtype;
    int userValue;
};

union Semun 
{
    int val;               /* value for SETVAL */
    struct semid_ds *buf;  /* buffer for IPC_STAT & IPC_SET */
    unsigned short *array; /* array for GETALL & SETALL */
    struct seminfo *__buf; /* buffer for IPC_INFO */
    void *__pad;
};
union Semun semun;

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

int itemGenerator()
{
    srand(time(0));
    return (rand() % 1000);
}
/*
void handler(int signum)
{
    shmctl(shmid, IPC_RMID, (struct shmid_ds*)0);
    sem_destroy(mutex);
    sem_destroy(full);
    sem_destroy(empty);
    exit(2);
} */

void produceItem(int *shm, int item)
{
    int lastIndex = shm[1];
    shm[lastIndex] = item;
    shm[1]++;
}

void producer(int *shm, int mutexSem, int emptySem, int fullSem)
{
    while (1)
    {
        int item = itemGenerator(); //generates random items of max 3 digits according to time seed

        if (semctl(fullSem, 0, GETVAL, semun) == userSize)
        {

            printf("Buffer is full, producer will sleep. \n");
            struct msgbuff result;
            int MSGRCV = msgrcv(consumerQueue, &result, sizeof(result.userValue), prodrcv, !IPC_NOWAIT);
            
            if (MSGRCV == -1)
            {
                perror("Producer couldn't receive wakeup message. \n");
            }
            else
            {
                printf("Producer has been awakened \n");
                sleep(3);
            }

        }

        down(emptySem);
        down(mutexSem);
        printf("The empty semaphore value is %d \n", semctl(emptySem, 0, GETVAL, semun));
        printf("the user size is %d \n", userSize);
        if (semctl(emptySem, 0, GETVAL, semun) == userSize -1)
        {
            produceItem(shm, item);
            printf("produced %d at %d \n", item, shm[1]);
            sleep(1);
            struct msgbuff tobeSent;
            tobeSent.userValue = 77;
            tobeSent.mtype = prodrcv;
            int MSGSND = msgsnd(producerQueue, &tobeSent, sizeof(tobeSent.userValue), !IPC_NOWAIT);
            if (MSGSND == -1)
            {
                perror("couldn't send wakeup message to consumer. \n");
            }
            else
            {
                printf("sent a wakeup message to consumer. \n");
            }
            
        }
        else
        {
            produceItem(shm, item); //adds the item to the first available index in the array, and increments that index
            sleep(3);               //sleeping to see the output
            printf("normal produced %d at %d \n", item, shm[1]);
        }

        up(mutexSem); //up mutex
        up(fullSem);  //up full
    }
}

/* arg for semctl system calls. */

int main()
{
    //signal(SIGINT, handler);
    signal(SIGALRM,SIG_IGN);

    
    //int userValQueue = msgget(userVal, IPC_CREAT | 0666); //userVal contains the user size that the producer will send to the consumer
    producerQueue = msgget(prod, IPC_CREAT | 0666);       //produceQueue contains the messages that the producer will send to wake up the consumer
    consumerQueue = msgget(cons, IPC_CREAT | 0666);       //consumerQueue contains the messages that the consumer will send to wake up the producer

    //User inputs the size of the buffer
    int sharedMemorySize = 2 * sizeof(int);
    printf("Enter the size of the buffer you need: \n");
    scanf("%d", &userSize);
    sharedMemorySize += userSize * sizeof(int);

    //sends a message to the consumer including the buffer size
    //struct msgbuff mymessage;
    //mymessage.mtype = userValType;
    //mymessage.userValue = sharedMemorySize;
    //int MSGSND = msgsnd(userValQueue, &mymessage, sizeof(mymessage.userValue), IPC_NOWAIT);

    //creates a shared memory segment with the user input size + 2 for array size and Last available index
    shmid = shmget(sharedMemory, sharedMemorySize, IPC_CREAT | 0666);
    if (shmid < 0)
    {
        perror("Shared Memory Get:");
        exit(1);
    }
    else
    {
        printf("Shared Memory allocated successfully. \n");
    }
    int *shm = (int *)shmat(shmid, 0, 0);
    if (shm == (int *)-1)
    {
        perror("failed to attach");
    }

    //saving the array size in the first location
    shm[0] = userSize + 2;
    shm[1] = 2; //saving the first available index in the array

    //Create full, empty and mutex semaphores
    int mutexSem = semget(mutexKey, 1, 0666 | IPC_CREAT);
    int emptySem = semget(emptyKey, 1, 0666 | IPC_CREAT);
    int fullSem = semget(fullKey, 1, 0666 | IPC_CREAT);
    if (mutexSem == -1 || emptySem == -1 || fullSem == -1)
    {
        perror("Error in creating Semaphore");
        exit(-1);
    }

    //initialise the mutex semaphore with 1
    semun.val = 1;
    if (semctl(mutexSem, 0, SETVAL, semun) == -1)
    {
        perror("Error in mutex semctl");
    }

    //initialise the empty semaphore with array size
    semun.val = userSize;
    if (semctl(emptySem, 0, SETVAL, semun) == -1)
    {
        perror("Error in empty semctl");
    }

    //initilaise the full semaphore with zero
    semun.val = 0;
    if (semctl(fullSem, 0, SETVAL, semun) == -1)
    {
        perror("Error in empty semctl");
    }

    //start the producer
    producer(shm, mutexSem, emptySem, fullSem);

    shmctl(shmid, IPC_RMID, (struct shmid_ds *)0);

    return 0;
}
