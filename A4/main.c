#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <string.h>


sem_t junsem[4];
pthread_cond_t juncv[4];
pthread_mutex_t junlk[4];

int turn[4] = {true, true, true, true};
int i = 0;

int deadlock = 0;
pthread_mutex_t lanelk;
pthread_cond_t deadlk_cross;

pthread_mutex_t deadlk_table_lk;
int deadlk[4] = {0, 0, 0, 0};   // atmost one thread is at a junction so no race condition really

pthread_mutex_t rng_mutex;

int thread_safe_rng(int min, int max) {
    pthread_mutex_lock(&rng_mutex);
    int r = rand();
    pthread_mutex_unlock(&rng_mutex);
    return min + r % max;
}

char *get_dir(int jid) {
    switch(jid) {
        case 0:
            return "North";
        case 1:
            return "West";
        case 2:
            return "South";
        case 3:
            return "East";
        default:
            fprintf(stderr, "error: unexpected junction id : %d", jid);
            exit(1);
    }
}

int *get_jid(char dir) {
    int *retval = malloc(sizeof(int));
    switch (dir) {
        case 'N':
            *retval = 0;
            break;
        case 'W':
            *retval = 1;
            break;
        case 'S':
            *retval = 2;
            break;
        case 'E':
            *retval = 3;
            break;
        default:
            fprintf(stderr, "error: invalid direction `%c`", dir);
            exit(1);                      
    }
    return retval;
}

bool isdeadlock() {
    pthread_mutex_lock(&deadlk_table_lk);
    bool res = deadlk[0] == 1 &&  deadlk[1] == 1 && deadlk[2] == 1 && deadlk[3] == 1;
    pthread_mutex_unlock(&deadlk_table_lk);
    return res;
}

/* TODO : can add global vars, structs, functions etc */

void *arriveLane(void *arg) {  // pass the junction number of the thread/train
    /* TODO: add code here */
    int jid = *(int*) arg;
    pthread_mutex_lock(&junlk[jid]);
    while (!turn[jid])
        pthread_cond_wait(&juncv[jid], &junlk[jid]);
    sem_wait(&junsem[jid]);
    turn[jid] = false;
    pthread_mutex_lock(&deadlk_table_lk);
    deadlk[jid] = 1;
    pthread_mutex_unlock(&deadlk_table_lk);
    pthread_mutex_unlock(&junlk[jid]);
}

void *crossLane(void *arg) {
    /* TODO: add code here */
    int jid = * (int *) arg;
    sem_wait(&junsem[(jid+1)%4]);
    pthread_mutex_lock(&deadlk_table_lk);
    deadlk[jid] = -1;
    pthread_mutex_unlock(&deadlk_table_lk);
    if (deadlock) 
        pthread_mutex_lock(&lanelk);
    usleep(1000 * thread_safe_rng(500, 1000)); // take 500-1000 ms to cross the lane
}

void *exitLane(void *arg) {
    /* TODO: add code here */
    int jid = * (int *) arg;
    turn[jid] = true;
    pthread_cond_signal(&juncv[jid]);
    sem_post(&junsem[jid]);
    if (!deadlock) 
        sem_post(&junsem[ (jid + 1) % 4 ]);
    if (deadlock) {
        pthread_mutex_unlock(&lanelk);
        deadlock = 0;
    }
    pthread_cond_signal(&deadlk_cross);
}



void *trainThreadFunction(void* arg)
{
    /* TODO extract arguments from the `void* arg` */
    int jid = * (int *) arg;
    usleep(thread_safe_rng(0, 10000)); // start at random time

    char* trainDir = get_dir(jid); // TODO set the direction of the train: North/South/East/West.

    arriveLane(arg);
    printf("Train Arrived at the lane from the %s direction\n", trainDir);

    crossLane(arg);

    printf("Train Exited the lane from the %s direction\n", trainDir);
    exitLane(arg);
    free(arg);
}

void *deadLockResolverThreadFunction(void * arg) {
    /* TODO extract arguments from the `void* arg` */
    while (1) {
        /* TODO add code to detect deadlock and resolve if any */
        deadlock = isdeadlock() ? 1 : 0;
        if (deadlock) {            
            printf("Deadlock detected. Resolving deadlock...\n");
            /* TODO add code to resolve deadlock */
            // allow train no. i to pass through
            pthread_mutex_lock(&lanelk);
            sem_post(&junsem[ (i + 1) % 4 ]);
            pthread_cond_wait(&deadlk_cross, &lanelk);
            // train has exited
            i = (i + 1) % 4;
            pthread_mutex_unlock(&lanelk);
        }
        usleep(1000 * 500); // sleep for 500 ms
    }
}




int main(int argc, char *argv[]) {

    if (argc != 2) {
        printf("Usage: ./main <train dirs: [NSWE]+>\n");
        return 1;
    }

    srand(time(NULL));

    pthread_mutex_init(&rng_mutex, NULL);
    for(int i = 0;i < 4;i++) {
        sem_init(&junsem[i], 0, 1);
        pthread_cond_init(&juncv[i], NULL);
        pthread_mutex_init(&junlk[i], NULL);
    }
    pthread_mutex_init(&lanelk, NULL);
    pthread_cond_init(&deadlk_cross, NULL);

    /* TODO create a thread for deadLockResolverThreadFunction */
    pthread_t deadlk;
    pthread_create(&deadlk, NULL, deadLockResolverThreadFunction, NULL);

    char* train = argv[1];
    pthread_t train_thread[strlen(train)];
    int num_trains = 0;

    while (train[num_trains] != '\0') {
        char trainDir = train[num_trains];

        if (trainDir != 'N' && trainDir != 'S' && trainDir != 'E' && trainDir != 'W') {
            printf("Invalid train direction: %c\n", trainDir);
            printf("Usage: ./main <train dirs: [NSEW]+>\n");
            return 1;
        }

        /* TODO create a thread for the train using trainThreadFunction */
        pthread_create(&train_thread[num_trains], NULL, trainThreadFunction, get_jid(trainDir));
        num_trains++;
    }

    /* TODO: join with all train threads*/
    for(int k = 0;k < num_trains;k++) {
        pthread_join(train_thread[k], NULL);
    }
    return 0;
}