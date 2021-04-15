#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

#define SIGUSR1 10


struct signal_time {
    int signal;
    int time;
};

int get_time();
struct signal_time st_array[10];
struct shared_val *sharednum;
pthread_mutexattr_t attr;


double randNum(int min, int max) {
    long interval = (long) ((rand() % (max - min + 1)) + min);
    return interval;
}

void signal_generator() {
    while (1) {
        sleep(0.5);

        double rand_interval = randNum(10, 100);

        int signal = randSignal();
        if (signal == SIGUSR1){
            pthread_mutex_lock(&(sharednum->mutex));

            sharednum->sigusr1_sent++; sharednum->total_signals++; pthread_mutex_unlock(&(sharednum->mutex));
        }
    }
}

void signal_handler(int signal) {

    if (signal == SIGUSR1){

        pthread_mutex_lock(&(sharednum->mutex));

        sharednum->sigusr1_received++; sharednum->total_signals++;
        pthread_mutex_unlock(&(sharednum->mutex));
    }
}

void signal_handler_main() {

    while(1) {
        sleep(1);
    }
}

int signal_reporter_main() {
    int return_val = 0;
    int signal;

    while(1) {
        sharednum->total_signals++;

        if(signal == SIGUSR1) {
            st_array[sharednum->signal_counter].time = get_time();
            st_array[sharednum->signal_counter].signal = SIGUSR1;
            sharednum->sigusr1_report_received++;
        }
    }
}

