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
#define SIGUSR1 8
#define SIGUSR2 8
#define SIGTERM 8


struct signal_time {
    int signal, time;
};

struct shared_val {
    int sigusr1_sent, sigusr1_received, sigusr2_sent, sigusr2_received, sigusr1_report_received, sigusr2_report_received, signal_counter ;
    pthread_mutex_t mutex;
};

void block_sigusr();
void unblock_sigusr();
void block_sigusr1();
void block_sigusr2();

double randNum(int min, int max);
int randSignal();
int sleep_milli(long given_time);
int init_mutex();

void signal_generator();
void signal_handler(int signal);
void signal_handler_main();

void signal_reporter_main();
int get_time();
void print_current_time();
void print_avg_time_gap();
void print_results();
struct signal_time st_array[10];


struct shared_val *shm_ptr;
pthread_mutexattr_t attr;

int main() {

    int shm_id;
    pid_t pid;

    shm_id = shmget(IPC_PRIVATE, sizeof(struct shared_val), IPC_CREAT | 0666);
    assert(shm_id >= 0);
    shm_ptr = (struct shared_val *) shmat(shm_id, NULL, 0);
    assert(shm_ptr != (struct shared_val *) -1);

    shm_ptr->sigusr1_sent = 0;
    shm_ptr->sigusr1_received = 0;
    shm_ptr->sigusr2_sent = 0;
    shm_ptr->sigusr2_received = 0;
    shm_ptr->sigusr1_report_received = 0;
    shm_ptr->sigusr2_report_received = 0;
    shm_ptr->signal_counter = 0;

    init_mutex();
    block_sigusr();

    for (size_t i = 0; i < num_pids; i++) {
        PIDs[i] = fork();
        if (PIDs[i] < 0) {
            puts("failed");


        } else if (PIDs[i] == 0) {

            if (i == 0 || i == 1 || i == 2) {
                signal_generator();

            } else if (i == 3 || i == 4) {
                unblock_sigusr();
                block_sigusr2();
                signal(SIGUSR1, signal_handler);
                signal_handler_main();

            } else if (i == 5 || i == 6) {
                unblock_sigusr(); block_sigusr1();
                signal(SIGUSR2, signal_handler); signal_handler_main();

            } else {
                signal_reporter_main();
            }
        }
    }

    sleep(30);

}
    double randNum(int min, int max) {
        long interval = (long) ((rand() % (max - min + 1)) + min);
        return interval;
    }

    int randSignal() {
        int n = randNum(1, 2);
        if(n == 1) { return SIGUSR1;
        } else { return SIGUSR2;
        }
    }

    int sleep_milli(long given_time) {
        int ret; struct timespec ts;

        assert(given_time >= 0);
        ts.tv_sec = given_time / 1000; ts.tv_nsec = (given_time % 1000) * 1000000;

    }

    int init_mutex() {
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&(shm_ptr->mutex), &attr);

        return 1;
    }

    void signal_generator() {

        while (1) {

            sleep(1);

            double rand_interval = randNum(10, 100);
            sleep_milli(rand_interval);

            int signal = randSignal();
            kill(0, signal);

            if (signal == SIGUSR1){
                pthread_mutex_lock(&(shm_ptr->mutex)); shm_ptr->sigusr1_sent++;
                pthread_mutex_unlock(&(shm_ptr->mutex));

            } else {

                pthread_mutex_lock(&(shm_ptr->mutex)); shm_ptr->sigusr2_sent++;
                pthread_mutex_unlock(&(shm_ptr->mutex));
            }
        }


    void signal_handler(int signal) {
        if (signal == SIGUSR1){
            pthread_mutex_lock(&(shm_ptr->mutex));

            shm_ptr->sigusr1_received++;
            pthread_mutex_unlock(&(shm_ptr->mutex));

        }
    }



    void signal_reporter_main() {

        sigset_t sigset;
        int return_val = 0; int signal;
        sigemptyset(&sigset);
        sigaddset(&sigset, SIGUSR1); sigaddset(&sigset, SIGUSR2);

        while(1) {
            return_val = sigwait(&sigset, &signal);

            if(shm_ptr->signal_counter == 10) {
                print_current_time(); print_results(); print_avg_time_gap();
                shm_ptr->signal_counter = 0;
            }

            if(signal == SIGUSR1) {

                st_array[shm_ptr->signal_counter].time = get_time();
                st_array[shm_ptr->signal_counter].signal = SIGUSR1;


                shm_ptr->sigusr1_report_received++;

            } else {

                st_array[shm_ptr->signal_counter].time = get_time();
                st_array[shm_ptr->signal_counter].signal = SIGUSR2;
                shm_ptr->sigusr2_report_received++;
            }
            shm_ptr->signal_counter++;

        }
    }

    int get_time() {
        time_t rawtime; struct tm * timeinfo;
        time ( &rawtime ); timeinfo = localtime ( &rawtime );
        return timeinfo->tm_sec;
    }

    void print_current_time() {
        time_t rawtime; struct tm * timeinfo;
        time ( &rawtime ); timeinfo = localtime ( &rawtime );
        printf ("\n\ncurrent system time: %d:%d:%d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    }

    void print_avg_time_gap() {

        int num_1 = 0; int num_2 = 0; int total_time_1 = 0; int total_time_2 = 0; int prev_time_1 = 0; int prev_time_2 = 0;

        for(size_t i = 0; i < 10; i++) {

            if(st_array[i].signal == SIGUSR1) { // SIGUSR1

                if(num_1 == 0) {
                    prev_time_1 = st_array[i].time;
                    i++;
                }
                num_1++;
                int temp = st_array[i].time - prev_time_1; total_time_1 += temp; prev_time_1 = st_array[i].time;

            } else {
                if(num_2 == 0) {
                    prev_time_2 = st_array[i].time;
                    i++;
                }
                num_2++;
                int temp = st_array[i].time - prev_time_2; total_time_2 += temp; prev_time_2 = st_array[i].time;
            }
        }

        double avg_1 = total_time_1/(double)num_1; double avg_2 = total_time_2/(double)num_2;
        printf("\naverage time for siguser#1: %f\naverage time for siguser#2: %f", avg_1, avg_2);

    }

    void print_results() {
        printf("%d %d", shm_ptr->sigusr1_sent, shm_ptr->sigusr2_sent);
        printf(" %d %d", shm_ptr->sigusr1_received, shm_ptr->sigusr2_received);
        printf(" %d %d", shm_ptr->sigusr1_report_received, shm_ptr->sigusr2_report_received);
        fflush(stdout);

    }