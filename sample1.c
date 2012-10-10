#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>

#define CURRENT_MODE    0
#define DEBUG_MODE      1

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;

void* thread1(void* arg);
void* thread2(void* arg);

int main()
{
    pthread_t thread;
    pthread_create(&thread, NULL, thread2, NULL); 
    thread1(0);
    pthread_join(thread, NULL);

    return 0;
}

void* thread1(void* arg)
{
    puts ("thread1-1");
    if (CURRENT_MODE == DEBUG_MODE) fprintf(stderr, "thread: %u thread1-1\n", pthread_self());
    pthread_mutex_lock(&mutex1);
    puts ("thread1-2");
    if (CURRENT_MODE == DEBUG_MODE) fprintf(stderr, "thread: %u thread1-2\n", pthread_self());
    sched_yield();
    pthread_mutex_lock(&mutex2);
    puts ("thread1-3");
    if (CURRENT_MODE == DEBUG_MODE) fprintf(stderr, "thread: %u thread1-3\n", pthread_self());
    pthread_mutex_unlock(&mutex2);
    puts ("thread1-4");
    if (CURRENT_MODE == DEBUG_MODE) fprintf(stderr, "thread: %u thread1-4\n", pthread_self());
    pthread_mutex_unlock(&mutex1);
}

void* thread2(void* arg)
{
    puts ("thread2-1");
    if (CURRENT_MODE == DEBUG_MODE) fprintf(stderr, "thread: %u thread2-1\n", pthread_self());
    pthread_mutex_lock(&mutex2);
    puts ("thread2-2");
    if (CURRENT_MODE == DEBUG_MODE) fprintf(stderr, "thread: %u thread2-2\n", pthread_self());
    sched_yield();
    pthread_mutex_unlock(&mutex2);
    puts ("thread2-3");
    if (CURRENT_MODE == DEBUG_MODE) fprintf(stderr, "thread: %u thread2-3\n", pthread_self());
    pthread_mutex_lock(&mutex1);
    puts ("thread2-4");
    if (CURRENT_MODE == DEBUG_MODE) fprintf(stderr, "thread: %u thread2-4\n", pthread_self());
    pthread_mutex_unlock(&mutex1);
}
