
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

#define num_pids 8
#define max_time 10
#define SIGUSR1 10
#define SIGUSR2 12
#define SIGTERM 15

// shared memory struct
struct shared_val {
    int sigusr1_sent;
    int sigusr1_received;
    int sigusr2_sent;
    int sigusr2_received;

    int sigusr1_report_received;
    int sigusr2_report_received;
    int signal_counter;

    pthread_mutex_t mutex;
};

// reporting process struct
struct signal_time {
    int signal;
    int time;
};

struct shared_val *shm_ptr;
pthread_mutexattr_t attr;

// signal masking functions
void block_sigusr();
void unblock_sigusr();
void block_sigusr1();
void block_sigusr2();

// init functions
double randNum(int min, int max);
int randSignal();
int sleep_milli(long given_time);
int init_mutex();

// signal generator/handlers
void signal_generator();
void signal_handler(int signal);
void signal_handler_main();

// signal reporting functions
void signal_reporter(int signal);
void signal_reporter_main();
int get_time();
void print_current_time();
void print_avg_time_gap();
void print_results();
struct signal_time st_array[10];

int PIDs[num_pids]; // process ID array
int total_time;

int main() {

    int shm_id;
    pid_t pid;

    shm_id = shmget(IPC_PRIVATE, sizeof(struct shared_val), IPC_CREAT | 0666); // created shared mem region
    assert(shm_id >= 0); // error check memory creation
    shm_ptr = (struct shared_val *) shmat(shm_id, NULL, 0); // attach memory
    assert(shm_ptr != (struct shared_val *) -1); // error check memory attachment

    shm_ptr->sigusr1_sent = 0;
    shm_ptr->sigusr1_received = 0;
    shm_ptr->sigusr2_sent = 0;
    shm_ptr->sigusr2_received = 0;
    shm_ptr->sigusr1_report_received = 0;
    shm_ptr->sigusr2_report_received = 0;
    shm_ptr->signal_counter = 0;

    init_mutex(); // initialize mutex
    block_sigusr();

    for(size_t i = 0; i < num_pids; i++) {

        PIDs[i] = fork();

        if (PIDs[i] < 0) {
            puts("fork failed");
            exit(0);

        } else if (PIDs[i] == 0) { // child

            if(i == 0 || i == 1 || i == 2) { // signal generating process
                signal_generator();

            } else if (i == 3 || i == 4) { // signal handling process for SIGUSR1
                // unblock
                unblock_sigusr();
                // block SIGUSR2
                block_sigusr2();
                // set signal_handler to receive SIGUSR1
                signal(SIGUSR1, signal_handler);
                // call signal handler function with SIGUSR1
                signal_handler_main();

            } else if (i == 5 || i == 6) { // signal handling process for SIGUSR2
                // unblock
                unblock_sigusr();
                // block SIGUSR1
                block_sigusr1();
                // set signal_handler to receive SIGUSR2
                signal(SIGUSR2, signal_handler);
                // call signal handler function with SIGUSR2
                signal_handler_main();

            } else { // reporting process
                // call signal reporting function
                signal_reporter_main();
            }
        }
    }

    sleep(30);
    puts("killing children");
    for (int i = 0; i < 8; i++){
        kill(PIDs[i], SIGTERM);
    }

    shmdt(shm_ptr);
    exit(0);

}

void block_sigusr() {
    sigset_t sigset;
    sigemptyset(&sigset); // initalize set to empty
    sigaddset(&sigset, SIGUSR1); // add SIGUSR1 to set
    sigaddset(&sigset, SIGUSR2); // add SIGUSR2 to set
    sigprocmask(SIG_BLOCK, &sigset, NULL); // modify mask

}

void unblock_sigusr() {
    sigset_t sigset;
    sigemptyset(&sigset); // initalize set to empty
    sigaddset(&sigset, SIGUSR1); // add SIGUSR1 to set
    sigaddset(&sigset, SIGUSR2); // add SIGUSR2 to set
    sigprocmask(SIG_UNBLOCK, &sigset, NULL); // modify mask
}

void block_sigusr1() {
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR1); // add SIGUSR1 to set
    sigprocmask(SIG_BLOCK, &sigset, NULL);
}

void block_sigusr2() {
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR2); // add SIGUSR2 to set
    sigprocmask(SIG_BLOCK, &sigset, NULL);

}

// returns a random number between min and max
double randNum(int min, int max) {
    long interval = (long) ((rand() % (max - min + 1)) + min);
    return interval;
}

// randomly returns either SIGUSR1 or SIGUSR2
int randSignal() {
    int n = randNum(1, 2);
    if(n == 1) {
        return SIGUSR1;
    } else {
        return SIGUSR2;
    }

}

// sleeps for a given number of milliseconds, returns 1 if successful
int sleep_milli(long given_time) {

    int ret;
    struct timespec ts;

    assert(given_time >= 0);
    ts.tv_sec = given_time / 1000;
    ts.tv_nsec = (given_time % 1000) * 1000000;
    do {
        ret = nanosleep(&ts, &ts);
    } while (ret && errno == EINTR);

}

int init_mutex() {
    // init mutex
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&(shm_ptr->mutex), &attr);

    return 1; // success
}

// increments signal sent counters
void signal_generator() {

    while (1) {

        sleep(1);

        double rand_interval = randNum(10, 100); // generate random interval
        //printf("\nrand interval: %lf", rand_interval);
        sleep_milli(rand_interval); // sleep random interval

        int signal = randSignal(); // randomly choose between SIGUSR1 or SIGUSR2
        kill(0, signal); // send that signal to all children

        if (signal == SIGUSR1){ // SIGUSR1

            //acquire lock
            pthread_mutex_lock(&(shm_ptr->mutex));

            shm_ptr->sigusr1_sent++; //increment signal counter

            // release lock
            pthread_mutex_unlock(&(shm_ptr->mutex));

        } else { // SIGUSR2

            //acquire lock
            pthread_mutex_lock(&(shm_ptr->mutex));

            shm_ptr->sigusr2_sent++; //increment signal counter

            // release lock
            pthread_mutex_unlock(&(shm_ptr->mutex));
        }
    }

}

// increments signal received counters
void signal_handler(int signal) {

    if (signal == SIGUSR1){

        //acquire lock
        pthread_mutex_lock(&(shm_ptr->mutex));

        shm_ptr->sigusr1_received++; //increment signal counter

        // release lock
        pthread_mutex_unlock(&(shm_ptr->mutex));

    } else { // SIGUSR2

        //acquire lock
        pthread_mutex_lock(&(shm_ptr->mutex));

        shm_ptr->sigusr2_received++; //increment signal counter

        // release lock
        pthread_mutex_unlock(&(shm_ptr->mutex));

    }

}

// signal handler process
void signal_handler_main() {

    while(1) {
        sleep(1);
    }

}


// signal reporting process
void signal_reporter_main() {

    sigset_t sigset;
    int return_val = 0;
    int signal;
    sigemptyset(&sigset);
    // initalize set to empty
    sigaddset(&sigset, SIGUSR1);
    sigaddset(&sigset, SIGUSR2);

    while(1) {

        return_val = sigwait(&sigset, &signal);

        // every 10 signals
        if(shm_ptr->signal_counter == 10) {
            print_current_time();
            print_results();
            print_avg_time_gap();
            shm_ptr->signal_counter = 0;
        }

        if(signal == SIGUSR1) { // SIGUSR1

            st_array[shm_ptr->signal_counter].time = get_time();
            st_array[shm_ptr->signal_counter].signal = SIGUSR1;

            // increment counter
            shm_ptr->sigusr1_report_received++;

        } else { // SIGUSR2

            st_array[shm_ptr->signal_counter].time = get_time();
            st_array[shm_ptr->signal_counter].signal = SIGUSR2;

            // increment counter
            shm_ptr->sigusr2_report_received++;
        }
        shm_ptr->signal_counter++;

    }
}

int get_time() {
    time_t rawtime;
    struct tm * timeinfo;
    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    //printf ("\ncurrent system time: %d:%d:%d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    return timeinfo->tm_sec;
}

void print_current_time() {
    time_t rawtime;
    struct tm * timeinfo;
    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    printf ("\n\ncurrent system time: %d:%d:%d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
}

void print_avg_time_gap() {

    int num_1 = 0;
    int num_2 = 0;
    int total_time_1 = 0;
    int total_time_2 = 0;
    int prev_time_1 = 0;
    int prev_time_2 = 0;

    for(size_t i = 0; i < 10; i++) {

        if(st_array[i].signal == SIGUSR1) { // SIGUSR1

            if(num_1 == 0) {
                prev_time_1 = st_array[i].time;
                i++;
            }
            num_1++;
            int temp = st_array[i].time - prev_time_1;
            total_time_1 += temp;
            prev_time_1 = st_array[i].time;

        } else { // SIGUSR2
            if(num_2 == 0) {
                prev_time_2 = st_array[i].time;
                i++;
            }
            num_2++;
            int temp = st_array[i].time - prev_time_2;
            total_time_2 += temp;
            prev_time_2 = st_array[i].time;
        }
    }
    //printf("\ntotal_1: %d total_time_1: %d", num_1, total_time_1);
    //printf("\ntotal_2: %d total_time_2: %d", num_2, total_time_2);

    double avg_1 = total_time_1/(double)num_1;
    double avg_2 = total_time_2/(double)num_2;
    printf("\naverage time between SIGUSR1: %f\naverage time between SIGUSR2: %f", avg_1, avg_2);

}

void print_results() {
    printf("\nsigusr1_sent: %d\nsigusr2_sent: %d", shm_ptr->sigusr1_sent, shm_ptr->sigusr2_sent);
    printf("\nsigusr1_received: %d\nsigusr2_received: %d", shm_ptr->sigusr1_received, shm_ptr->sigusr2_received);
    printf("\nsigusr1_report_received: %d\nsigusr2_report_received: %d", shm_ptr->sigusr1_report_received, shm_ptr->sigusr2_report_received);
    fflush(stdout);

}