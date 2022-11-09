#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>


pthread_mutex_t rng_mutex;

int thread_safe_rng(int min, int max) {
    pthread_mutex_lock(&rng_mutex);
    int r = rand();
    pthread_mutex_unlock(&rng_mutex);
    return min + r % max;
}

/* TODO : can add global vars, structs, functions etc */

void arriveLane(/* TODO you can add parameters to the function*/) {
    /* TODO: add code here */
}

void crossLane(/* TODO you can add parameters to the function*/) {
    usleep(1000 * thread_safe_rng(500, 1000)); // take 500-1000 ms to cross the lane

    /* TODO: add code here */
}

void exitLane(/* TODO you can add parameters to the function*/) {
    /* TODO: add code here */
}



void trainThreadFunction(void* arg)
{
    /* TODO extract arguments from the `void* arg` */
    usleep(thread_safe_rng(0, 10000)); // start at random time
    arriveLane(/* TODO you can add arguments to the function*/);

    char* trainDir = NULL; // TODO set the direction of the train: North/South/East/West.

    arriveLane(/* TODO you can add arguments to the function*/);
    printf("Train Arrived at the lane from the %s direction", trainDir);

    crossLane(/* TODO you can add arguments to the function*/);

    exitLane(/* TODO you can add arguments to the function*/);
    printf("Train Exited the lane from the %s direction", trainDir);
}

void deadLockResolverThreadFunction(void * arg) {
    /* TODO extract arguments from the `void* arg` */
    while (1) {
        /* TODO add code to detect deadlock and resolve if any */

        int deadLockDetected = 0; // TODO set to 1 if deadlock is detected

        if (deadLockDetected) {
            printf("Deadlock detected. Resolving deadlock...\n");
            /* TODO add code to resolve deadlock */
        }

        usleep(1000 * 500); // sleep for 500 ms
    }
}






int main(int argc, char *argv[]) {


    srand(time(NULL));

    if (argc != 2) {
        printf("Usage: ./main <train dirs: [NSWE]+>\n");
        return 1;
    }

    pthread_mutex_init(&rng_mutex, NULL);


    /* TODO create a thread for deadLockResolverThreadFunction */

    char* train = argv[1];

    int num_trains = 0;

    while (train[num_trains] != '\0') {
        char trainDir = train[num_trains];

        if (trainDir != 'N' && trainDir != 'S' && trainDir != 'E' && trainDir != 'W') {
            printf("Invalid train direction: %c\n", trainDir);
            printf("Usage: ./main <train dirs: [NSEW]+>\n");
            return 1;
        }

        /* TODO create a thread for the train using trainThreadFunction */

        num_trains++;
    }

    /* TODO: join with all train threads*/



    return 0;
}