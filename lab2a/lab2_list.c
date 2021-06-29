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
#include "SortedList.h"

int opt_yield = 0;
char *yield_val;
int sync_flag = 0;
char *sync_val;

int threads_val = 1;
int iterations_val = 1;
SortedListElement_t *listhead, *pool;

pthread_mutex_t mutex;
long lock = 0;


void signalHandler(int sig) {
    if (sig == SIGSEGV) {
        fprintf(stderr, "Captured segmentation fault\n");
        exit(2);
    }
}


void *thread_worker(void *arg) {
    int threadNum = *((int *) arg);
    int startIndex = threadNum * iterations_val;
    
    if (sync_flag == 1) {
        if (strcmp(sync_val, "m") == 0) {
            for (int i = startIndex; i < startIndex + iterations_val; i++) {
                pthread_mutex_lock(&mutex);
                SortedList_insert(listhead, &pool[i]); //insert element
                pthread_mutex_unlock(&mutex);
            }
        }
        else if (strcmp(sync_val, "s") == 0) {
            for (int i = startIndex; i < startIndex + iterations_val; i++) {
                while (__sync_lock_test_and_set (&lock, 1));
                SortedList_insert(listhead, &pool[i]); //insert element
                __sync_lock_release(&lock);
            }
        }
    }
    else {
        for (int i = 0; i < iterations_val; i++)
            SortedList_insert(listhead, &pool[i]); //insert element
    }
    
    int length;
    length = SortedList_length(listhead); //check length
    if (length == -1) {
        fprintf(stderr, "Error list length error. \n");
        exit(2);
    }
    
    if (sync_flag == 1) {
        if (strcmp(sync_val, "m") == 0) {
            for (int i = startIndex; i < startIndex + iterations_val; i++) {
                pthread_mutex_lock(&mutex);
                SortedListElement_t* e = SortedList_lookup(listhead, pool[i].key); //lookup inserted element
                if (e == NULL) {
                    fprintf(stderr, "Error lookup. \n");
                    exit(2);
                }
                int r = SortedList_delete(e); //remove inserted element
                if (r == 1) {
                    fprintf(stderr, "Error delete. \n");
                    exit(2);
                }
                pthread_mutex_unlock(&mutex);
            }
            
        }
        else if (strcmp(sync_val, "s") == 0) {
            for (int i = startIndex; i < startIndex + iterations_val; i++) {
                while (__sync_lock_test_and_set (&lock, 1));
                SortedListElement_t* e = SortedList_lookup(listhead, pool[i].key); //lookup inserted element
                if (e == NULL) {
                    fprintf(stderr, "Error lookup. \n");
                    exit(2);
                }
                int r = SortedList_delete(e); //remove inserted element
                if (r == 1) {
                    fprintf(stderr, "Error delete. \n");
                    exit(2);
                }
                __sync_lock_release(&lock);
            }
        }
    }
    else {
        for (int i = 0; i < iterations_val; i++) {
            SortedListElement_t* e = SortedList_lookup(listhead, pool[i].key); //lookup inserted element
            if (e == NULL) {
                fprintf(stderr, "Error lookup. \n");
                exit(2);
            }
            int r = SortedList_delete(e); //remove inserted element
            if (r == 1) {
                fprintf(stderr, "Error delete. \n");
                exit(2);
            }
        }
    }
    
    return NULL;
}



char *generateKey() {
    char *res = (char *) malloc(4 * sizeof(char));
    srand(time(NULL));
    for (int i = 0; i < 3; i++) {
        res[i] = 'a' + (rand() % 26);
    }
    res[3] = '\0';
    return res;
}



void print_out(char *name, int threads, int iterations, int list_num, long long total_runtime) {
    int total_num_operation = threads * iterations * 3;
    long long avg_time = total_runtime/total_num_operation;
    fprintf(stdout, "%s,%d,%d,%d,%d,%lld,%lld\n", name, threads, iterations, list_num, total_num_operation, total_runtime, avg_time);
}



int main(int argc, char * argv[])
{
    int insert_flag = 0;
    int delete_flag = 0;
    int lookup_flag = 0;
    
    /* options for option parse */
    static struct option long_options[] = {
        {"threads", required_argument, NULL, 't'},
        {"iterations", required_argument, NULL, 'i'},
        {"yield", required_argument, NULL, 'y'},
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
                yield_val = optarg;
                break;
                
            case 's':
                sync_flag = 1;
                sync_val = optarg;
                break;
            
            default:
                fprintf(stderr, "Wrong usage of options, accepted options are [--threads=] [--iterations=] [--yield=] [--sync=]. \n");
                exit(1);
        }
    }
    
    if (opt_yield == 1) {
        if (strlen(yield_val) > 3) {
            fprintf(stderr, "Wrong usage of [--yield=], too many value applied. \n");
            exit(1);
        }
        
        for (size_t i = 0; i < strlen(yield_val); i++) {
            if (yield_val[i] == 'i') {
                if (insert_flag == 1) {
                    fprintf(stderr, "Wrong usage of [--yield], unrecognized value applied. \n");
                    exit(1);
                }
                insert_flag = 1;
                opt_yield |= INSERT_YIELD;
            }
            else if (yield_val[i] == 'd') {
                if (delete_flag == 1) {
                    fprintf(stderr, "Wrong usage of [--yield], unrecognized value applied. \n");
                    exit(1);
                }
                delete_flag = 1;
                opt_yield |= DELETE_YIELD;
            }
            else if (yield_val[i] == 'l') {
                if (lookup_flag == 1) {
                    fprintf(stderr, "Wrong usage of [--yield], unrecognized value applied. \n");
                    exit(1);
                }
                lookup_flag = 1;
                opt_yield |= LOOKUP_YIELD;
            }
            else {
                fprintf(stderr, "Wrong usage of [--yield], unrecognized value applied. \n");
                exit(1);
            }
        }
    }
    
    if (sync_flag == 1) {
        if (strcmp(sync_val, "m") == 0) {
            pthread_mutex_init(&mutex, NULL);
        }
        else if (strcmp(sync_val, "s") == 0) {
            ;
        }
        else {
            fprintf(stderr, "Wrong usage of [--sync=], unrecognized value applied. \n");
            exit(1);
        }
    }
    
    signal(SIGSEGV, signalHandler);
    
    listhead = (SortedListElement_t*) malloc(sizeof(SortedListElement_t));
    listhead->prev = listhead;
    listhead->next = listhead;
    listhead->key = NULL;
    
    pool = malloc(threads_val * iterations_val * sizeof(SortedListElement_t));
    for (int i = 0; i < threads_val * iterations_val; i++) {
        pool[i].key = generateKey();
    }
    
    // Notes the (high resolution) starting time for the run
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Starts the specified number of threads
    int index[threads_val];
    for (int i = 0; i < threads_val; i++) {
        index[i] = i;
    }
    
    pthread_t thread[threads_val];
    for (int i = 0; i < threads_val; i++) {
        pthread_create(thread+i, NULL, thread_worker, (index + i));
    }
    for (int i = 0; i < threads_val; i++)
        pthread_join(thread[i], NULL);
    
    // Wait for all threads to complete, and notes the (high resolution) ending time for the run
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    // Calculate total time
    long long total_runtime;
    total_runtime = 1000000000 * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec);
    
    // Printing out
    if (insert_flag == 0 && delete_flag == 0 && lookup_flag == 0) {        // none
        if (sync_flag == 1) {
            if (strcmp(sync_val, "m") == 0) {
                char *m_list_none_m;
                m_list_none_m = "list-none-m";
                print_out(m_list_none_m, threads_val, iterations_val, 1, total_runtime);
            }
            else if (strcmp(sync_val, "s") == 0) {
                char *m_list_none_s;
                m_list_none_s = "list-none-s";
                print_out(m_list_none_s, threads_val, iterations_val, 1, total_runtime);
            }
        }
        else {
            char *m_list_none_none;
            m_list_none_none = "list-none-none";
            print_out(m_list_none_none, threads_val, iterations_val, 1, total_runtime);
        }
    }
    else if (insert_flag == 1 && delete_flag == 0 && lookup_flag == 0) {        // i
        if (sync_flag == 1) {
            if (strcmp(sync_val, "m") == 0) {
                char *m_list_i_m;
                m_list_i_m = "list-i-m";
                print_out(m_list_i_m, threads_val, iterations_val, 1, total_runtime);
            }
            else if (strcmp(sync_val, "s") == 0) {
                char *m_list_i_s;
                m_list_i_s = "list-i-s";
                print_out(m_list_i_s, threads_val, iterations_val, 1, total_runtime);
            }
        }
        else {
            char *m_list_i_none;
            m_list_i_none = "list-i-none";
            print_out(m_list_i_none, threads_val, iterations_val, 1, total_runtime);
        }
    }
    else if (insert_flag == 0 && delete_flag == 1 && lookup_flag == 0) {        //d
        if (sync_flag == 1) {
            if (strcmp(sync_val, "m") == 0) {
                char *m_list_d_m;
                m_list_d_m = "list-d-m";
                print_out(m_list_d_m, threads_val, iterations_val, 1, total_runtime);
            }
            else if (strcmp(sync_val, "s") == 0) {
                char *m_list_d_s;
                m_list_d_s = "list-d-s";
                print_out(m_list_d_s, threads_val, iterations_val, 1, total_runtime);
            }
        }
        else {
            char *m_list_d_none;
            m_list_d_none = "list-d-none";
            print_out(m_list_d_none, threads_val, iterations_val, 1, total_runtime);
        }
    }
    else if (insert_flag == 0 && delete_flag == 0 && lookup_flag == 1) {        //l
        if (sync_flag == 1) {
            if (strcmp(sync_val, "m") == 0) {
                char *m_list_l_m;
                m_list_l_m = "list-l-m";
                print_out(m_list_l_m, threads_val, iterations_val, 1, total_runtime);
            }
            else if (strcmp(sync_val, "s") == 0) {
                char *m_list_l_s;
                m_list_l_s = "list-l-s";
                print_out(m_list_l_s, threads_val, iterations_val, 1, total_runtime);
            }
        }
        else {
            char *m_list_l_none;
            m_list_l_none = "list-l-none";
            print_out(m_list_l_none, threads_val, iterations_val, 1, total_runtime);
        }
    }
    else if (insert_flag == 1 && delete_flag == 1 && lookup_flag == 0) {        //id
        if (sync_flag == 1) {
            if (strcmp(sync_val, "m") == 0) {
                char *m_list_id_m;
                m_list_id_m = "list-id-m";
                print_out(m_list_id_m, threads_val, iterations_val, 1, total_runtime);
            }
            else if (strcmp(sync_val, "s") == 0) {
                char *m_list_id_s;
                m_list_id_s = "list-id-s";
                print_out(m_list_id_s, threads_val, iterations_val, 1, total_runtime);
            }
        }
        else {
            char *m_list_id_none;
            m_list_id_none = "list-id-none";
            print_out(m_list_id_none, threads_val, iterations_val, 1, total_runtime);
        }
    }
    else if (insert_flag == 1 && delete_flag == 0 && lookup_flag == 1) {        //il
        if (sync_flag == 1) {
            if (strcmp(sync_val, "m") == 0) {
                char *m_list_il_m;
                m_list_il_m = "list-il-m";
                print_out(m_list_il_m, threads_val, iterations_val, 1, total_runtime);
            }
            else if (strcmp(sync_val, "s") == 0) {
                char *m_list_il_s;
                m_list_il_s = "list-il-s";
                print_out(m_list_il_s, threads_val, iterations_val, 1, total_runtime);
            }
        }
        else {
            char *m_list_il_none;
            m_list_il_none = "list-il-none";
            print_out(m_list_il_none, threads_val, iterations_val, 1, total_runtime);
        }
    }
    else if (insert_flag == 0 && delete_flag == 1 && lookup_flag == 1) {        //dl
        if (sync_flag == 1) {
            if (strcmp(sync_val, "m") == 0) {
                char *m_list_dl_m;
                m_list_dl_m = "list-dl-m";
                print_out(m_list_dl_m, threads_val, iterations_val, 1, total_runtime);
            }
            else if (strcmp(sync_val, "s") == 0) {
                char *m_list_dl_s;
                m_list_dl_s = "list-dl-s";
                print_out(m_list_dl_s, threads_val, iterations_val, 1, total_runtime);
            }
        }
        else {
            char *m_list_dl_none;
            m_list_dl_none = "list-dl-none";
            print_out(m_list_dl_none, threads_val, iterations_val, 1, total_runtime);
        }
    }
    else if (insert_flag == 1 && delete_flag == 1 && lookup_flag == 1) {        //idl
        if (sync_flag == 1) {
            if (strcmp(sync_val, "m") == 0) {
                char *m_list_idl_m;
                m_list_idl_m = "list-idl-m";
                print_out(m_list_idl_m, threads_val, iterations_val, 1, total_runtime);
            }
            else if (strcmp(sync_val, "s") == 0) {
                char *m_list_idl_s;
                m_list_idl_s = "list-idl-s";
                print_out(m_list_idl_s, threads_val, iterations_val, 1, total_runtime);
            }
        }
        else {
            char *m_list_idl_none;
            m_list_idl_none = "list-idl-none";
            print_out(m_list_idl_none, threads_val, iterations_val, 1, total_runtime);
        }
    }
    
    for (int i = 0; i < threads_val * iterations_val; i++) {
        free((void *) pool[i].key);
    }
    free(pool);
    free(listhead);
}

