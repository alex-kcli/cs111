#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <sched.h>

long long counter = 0;
int opt_yield = 0;
int sync_flag;
char *sync_val;

pthread_mutex_t mutex;
long lock = 0;


void add(long long *pointer, long long value) {
    long long sum = *pointer + value;
    if (opt_yield)
        sched_yield();
    *pointer = sum;
}

void atomic_add(long long *pointer, long long value) {
    long long prev, sum;
    do {
        prev = *pointer;
        sum = prev + value;
        if(opt_yield){
            sched_yield();
        }
    } while (__sync_val_compare_and_swap (pointer, prev, sum) != prev);
}


void *thread_worker(void *arg) {
    int iteration = *((int *) arg);
    if (sync_flag == 1) {
        if (strcmp(sync_val, "m") == 0) {
            for (int i = 0; i < iteration; i++) {
                pthread_mutex_lock(&mutex);
                add(&counter, 1);
                pthread_mutex_unlock(&mutex);
            }
        }
        else if (strcmp(sync_val, "s") == 0) {
            for (int i = 0; i < iteration; i++) {
                while (__sync_lock_test_and_set (&lock, 1));
                add(&counter, 1);
                __sync_lock_release(&lock);
            }
        }
        else if (strcmp(sync_val, "c") == 0) {
            for (int i = 0; i < iteration; i++) {
                atomic_add(&counter, 1);
            }
        }
    }
    else {
        for (int i = 0; i < iteration; i++) {
            add(&counter, 1);
        }
    }
    
    if (sync_flag == 1) {
        if (strcmp(sync_val, "m") == 0) {
            for (int i = 0; i < iteration; i++) {
                pthread_mutex_lock(&mutex);
                add(&counter, -1);
                pthread_mutex_unlock(&mutex);
            }
        }
        else if (strcmp(sync_val, "s") == 0) {
            for (int i = 0; i < iteration; i++) {
                while (__sync_lock_test_and_set (&lock, 1));
                add(&counter, -1);
                __sync_lock_release(&lock);
            }
        }
        else if (strcmp(sync_val, "c") == 0) {
            for (int i = 0; i < iteration; i++) {
                atomic_add(&counter, -1);
            }
        }
    }
    else {
        for (int i = 0; i < iteration; i++) {
            add(&counter, -1);
        }
    }
    
    return NULL;
}


void print_out(char *name, int threads, int iterations, long long total_runtime, long long sum) {
    int total_num_operation = threads * iterations * 2;
    long long avg_time = total_runtime/total_num_operation;
    fprintf(stdout, "%s,%d,%d,%d,%lld,%lld,%lld\n", name, threads, iterations, total_num_operation, total_runtime, avg_time, sum);
}


int main(int argc, char * argv[])
{
    int threads_val = 1;
    int iterations_val = 1;
    
    /* options for option parse */
    static struct option long_options[] = {
        {"threads", required_argument, NULL, 't'},
        {"iterations", required_argument, NULL, 'i'},
        {"yield", no_argument, NULL, 'y'},
        {"sync", required_argument, NULL, 's'},
        {0, 0, 0, 0}
    };
    
    int ch;
    while ((ch = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
        switch (ch) {
            case 't':
                threads_val = atoi(optarg);
                break;
            
            case 'i':
                iterations_val = atoi(optarg);
                break;
            
            case 'y':
                opt_yield = 1;
                break;
                
            case 's':
                sync_flag = 1;
                sync_val = optarg;
                break;
            
            default:
                fprintf(stderr, "Wrong usage of options, accepted options are [--threads=] [--iterations=] [--yield] [--sync=]. \n");
                exit(1);
        }
    }
    

    
    if (sync_flag == 1) {
        if (strcmp(sync_val, "m") == 0) {
            pthread_mutex_init(&mutex, NULL);
        }
        else if (strcmp(sync_val, "s") == 0) {
            ;
        }
        else if (strcmp(sync_val, "c") == 0) {
            ;
        }
        else {
            fprintf(stderr, "Wrong usage of [--sync=], unrecognized value applied. \n");
            exit(1);
        }
    }
    
    // Notes the (high resolution) starting time for the run
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Starts the specified number of threads
    pthread_t thread[threads_val];
    
    for (int i = 0; i < threads_val; i++)
        pthread_create(thread+i, NULL, thread_worker, &iterations_val);
    for (int i = 0; i < threads_val; i++)
        pthread_join(thread[i], NULL);
    
    // Wait for all threads to complete, and notes the (high resolution) ending time for the run
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    // Calculate total time
    long long total_runtime;
    total_runtime = 1000000000 * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec);
    
    if (sync_flag == 1) {
        if (opt_yield == 1) {
            if (strcmp(sync_val, "m") == 0) {
                char *m_add_yield_m;
                m_add_yield_m = "add-yield-m";
                print_out(m_add_yield_m, threads_val, iterations_val, total_runtime, counter);
            }
            else if (strcmp(sync_val, "s") == 0) {
                char *m_add_yield_s;
                m_add_yield_s = "add-yield-s";
                print_out(m_add_yield_s, threads_val, iterations_val, total_runtime, counter);
            }
            else if (strcmp(sync_val, "c") == 0) {
                char *m_add_yield_c;
                m_add_yield_c = "add-yield-c";
                print_out(m_add_yield_c, threads_val, iterations_val, total_runtime, counter);
            }
        }
        else {
            if (strcmp(sync_val, "m") == 0) {
                char *m_add_m;
                m_add_m = "add-m";
                print_out(m_add_m, threads_val, iterations_val, total_runtime, counter);
            }
            else if (strcmp(sync_val, "s") == 0) {
                char *m_add_s;
                m_add_s = "add-s";
                print_out(m_add_s, threads_val, iterations_val, total_runtime, counter);
            }
            else if (strcmp(sync_val, "c") == 0) {
                char *m_add_c;
                m_add_c = "add-c";
                print_out(m_add_c, threads_val, iterations_val, total_runtime, counter);
            }
        }
    }
    else {
        if (opt_yield == 1) {
            char *m_add_yield_none;
            m_add_yield_none = "add-yield-none";
            print_out(m_add_yield_none, threads_val, iterations_val, total_runtime, counter);
        }
        else {
            char *m_add_none;
            m_add_none = "add-none";
            print_out(m_add_none, threads_val, iterations_val, total_runtime, counter);
        }
    }
    
}
