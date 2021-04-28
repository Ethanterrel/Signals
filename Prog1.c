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
#define SIGUSR2 12
#define SIGTERM 15
#define num_pids 8
#define SIGUSR1 10

struct signal_time {
    int signal, time;
};

struct shared_val {
    int sigusr1_sent, sigusr1_received, sigusr2_sent, sigusr2_received, sigusr1_report_received, sigusr2_report_received, signal_counter ;
    pthread_mutex_t mutex;
};

struct shared_val *shm_ptr;
pthread_mutexattr_t attr;
void block_sigusr(),unblock_sigusr(),block_sigusr1(),block_sigusr2();


double randNum(int min, int max);
int sleep_milli(long given_time),randSignal(), init_mutex();
void exit_program();

// signal generator/handlers
void signal_handler(int signal),signal_generator(),signal_handler_main();

// signal reporting functions
int signal_reporter_main(), get_time() ;
void print_current_time(), print_avg_time_gap(),print_results(); ;
struct signal_time st_array[10];

int PIDs[num_pids]; // process ID array

int main() {

    int shm_id;
    pid_t pid;

    shm_id = shmget(IPC_PRIVATE, sizeof(struct shared_val), IPC_CREAT | 0666); // created shared mem region
    assert(shm_id >= 0);
    shm_ptr = (struct shared_val *) shmat(shm_id, NULL, 0);
    assert(shm_ptr != (struct shared_val *) -1);

    shm_ptr->sigusr1_sent = 0,shm_ptr->sigusr1_received = 0, shm_ptr->sigusr2_sent = 0;
    shm_ptr->sigusr2_received = 0,shm_ptr->sigusr1_report_received = 0,shm_ptr->sigusr2_report_received = 0 ;
    shm_ptr->signal_counter = 0;
    init_mutex(); block_sigusr();

    for(size_t i = 0; i < num_pids; i++) {
        PIDs[i] = fork();
        if (PIDs[i] < 0) {
            puts("failed");

        } else if (PIDs[i] == 0) {

            if(i == 0 || i == 1 || i == 2) { // signal generating process
                signal_generator();

            } else if (i == 3 || i == 4) { // signal handling process for SIGUSR1
                unblock_sigusr(); block_sigusr2();
                signal(SIGUSR1, signal_handler), signal_handler_main() ;

            } else if (i == 5 || i == 6) { // signal handling process for SIGUSR2
                unblock_sigusr(); block_sigusr1();
                signal(SIGUSR2, signal_handler),signal_handler_main() ;

            } else { // reporting process
                if(signal_reporter_main() == -1) {
                }
            }
        }
    }

    //sleep(30);
    //printf("\ntotal signals: %d", shm_ptr->total_signals);
    //exit_program();
    while(1) {
        if(shm_ptr->total_signals >= 10000) {
        exit_program();
        }
        sleep(0.1);
    }
}

void exit_program() {
    for (int i = 0; i < 8; i++){
        kill(PIDs[i], SIGTERM);
    }
    shmdt(shm_ptr);
}


void block_sigusr() {
    sigset_t sigset; sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR1); // add SIG1
    sigaddset(&sigset, SIGUSR2); // add SIG2
    sigprocmask(SIG_BLOCK, &sigset, NULL);

}

void unblock_sigusr() {
    sigset_t sigset; sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR1); // add SIG1
    sigaddset(&sigset, SIGUSR2); // add SIG2
    sigprocmask(SIG_UNBLOCK, &sigset, NULL);
}

void block_sigusr1() {
    sigset_t sigset; sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR1); // add SIG1
    sigprocmask(SIG_BLOCK, &sigset, NULL);
}

void block_sigusr2() {
    sigset_t sigset; sigemptyset(&sigset);
    sigaddset(&sigset, SIGUSR2); // add SIG2
    sigprocmask(SIG_BLOCK, &sigset, NULL);

}

double randNum(int min, int max) {
    long interval = (long) ((rand() % (max - min + 1)) + min);
    return interval;
}

//returns either SIG1 or SIG2
int randSignal() {
    int n = randNum(1, 2);
    if(n == 1) { return SIGUSR1;
    } else { return SIGUSR2;
    }

}

int sleep_milli(long given_time) {
    int ret; struct timespec ts;

    assert(given_time >= 0);
    ts.tv_sec = given_time / 1000;
    ts.tv_nsec = (given_time % 1000) * 1000000;
    do { ret = nanosleep(&ts, &ts);
    } while (ret && errno == EINTR);

}

int init_mutex() {

    pthread_mutexattr_init(&attr); pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&(shm_ptr->mutex), &attr);
}

// increments signal sent counters
void signal_generator() {
    while (1) {
        sleep(0.5);

        double rand_interval = randNum(10, 100);

        sleep_milli(rand_interval); // sleep random interval

        int signal = randSignal(); // randomly choose between SIGUSR1 or SIGUSR2
        kill(0, signal);

        if (signal == SIGUSR1){
            pthread_mutex_lock(&(shm_ptr->mutex));
            shm_ptr->sigusr1_sent++; shm_ptr->total_signals++;
            pthread_mutex_unlock(&(shm_ptr->mutex));

        } else { // SIGUSR2
            pthread_mutex_lock(&(shm_ptr->mutex));
            shm_ptr->sigusr2_sent++; shm_ptr->total_signals++;
            pthread_mutex_unlock(&(shm_ptr->mutex));
        }
    }

}

void signal_handler(int signal) {
    if (signal == SIGUSR1){
        pthread_mutex_lock(&(shm_ptr->mutex));
        shm_ptr->sigusr1_received++; shm_ptr->total_signals++;
        pthread_mutex_unlock(&(shm_ptr->mutex));

    } else { // SIGUSR2
        pthread_mutex_lock(&(shm_ptr->mutex));
        shm_ptr->sigusr2_received++; shm_ptr->total_signals++;
        pthread_mutex_unlock(&(shm_ptr->mutex));
    }
}


void signal_handler_main() {
    while(1) {
        sleep(1);
    }

}


int signal_reporter_main() {
    sigset_t sigset; sigemptyset(&sigset); sigaddset(&sigset, SIGUSR1),sigaddset(&sigset, SIGUSR2);
    int return_val = 0,signal;

    while(1) {
        return_val = sigwait(&sigset, &signal); shm_ptr->total_signals++;

        // every 10 signals
        if(shm_ptr->signal_counter == 10) {
            print_current_time(),print_results(),print_avg_time_gap();
            shm_ptr->signal_counter = 0;
        }
        if(signal == SIGUSR1) {  //SIG1
            st_array[shm_ptr->signal_counter].time = get_time(); st_array[shm_ptr->signal_counter].signal = SIGUSR1;
            shm_ptr->sigusr1_report_received++;

        } else { // SIG2
            st_array[shm_ptr->signal_counter].time = get_time(); st_array[shm_ptr->signal_counter].signal = SIGUSR2;
            shm_ptr->sigusr2_report_received++;
        }
        shm_ptr->signal_counter++;

    }
}

int get_time() {
    struct tm * timeinfo;
    time_t rawtime; time ( &rawtime ); timeinfo = localtime ( &rawtime );
    return timeinfo->tm_sec;
}

void print_current_time() {
    struct tm * timeinfo;
    time_t rawtime; time ( &rawtime ); timeinfo = localtime ( &rawtime );
    printf ("\n\ncurrent time is: %d:%d:%d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
}

void print_avg_time_gap() {

    int num_1 = 0; int num_2 = 0;
    int total_time_1 = 0; int total_time_2 = 0;
    int prev_time_1 = 0; int prev_time_2 = 0;

    for(size_t i = 0; i < 10; i++) {

        if(st_array[i].signal == SIGUSR1) { // SIG1
            if(num_1 == 0) {
                prev_time_1 = st_array[i].time;
                i++;
            }
            num_1++;
            int temp = st_array[i].time - prev_time_1;
            total_time_1 += temp; prev_time_1 = st_array[i].time;

        } else { // SIGUSR2
            if(num_2 == 0) {
                prev_time_2 = st_array[i].time;
                i++;
            }
            num_2++;
            int temp = st_array[i].time - prev_time_2;
            total_time_2 += temp; prev_time_2 = st_array[i].time;
        }
    }

    double avg_1 = 0;
    double avg_2 = 0;
    if(num_1 > 0) { avg_1 = total_time_1/(double)num_1;
    }
    if(num_2 > 0) { avg_2 = total_time_2/(double)num_2;
    }

    printf("\naverage time between SIG1 is : %f\naverage time between SIG2: %f", avg_1, avg_2);

}

void print_results() {
    printf("\nSIGUSR1 received is : %d\nSIGUSR2 sent is %d", shm_ptr->sigusr1_sent, shm_ptr->sigusr2_sent);
    printf("\nSIGUSR1 received is: %d\nSIGUSR2 sent is %d", shm_ptr->sigusr1_received, shm_ptr->sigusr2_received);
    printf("\nSIGUSR1 report received is: %d\nSIGUSR2 report received is: %d", shm_ptr->sigusr1_report_received, shm_ptr->sigusr2_report_received);
    fflush(stdout);

}
