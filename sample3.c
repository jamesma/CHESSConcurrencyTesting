#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>

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

void randomSleep()
{
    sleep(rand()%2);
}

void* thread1(void* arg)
{
    puts ("thread1-1");
    randomSleep();
    pthread_mutex_lock(&mutex1);
    randomSleep();
    puts ("thread1-2");
    sched_yield();
    randomSleep();
    pthread_mutex_lock(&mutex2);
    randomSleep();
    puts ("thread1-3");
    randomSleep();
    pthread_mutex_unlock(&mutex2);
    randomSleep();
    puts ("thread1-4");
    randomSleep();
    pthread_mutex_unlock(&mutex1);
    randomSleep();
}

void* thread2(void* arg)
{
    puts ("thread2-1");
    randomSleep();
    pthread_mutex_lock(&mutex2);
    randomSleep();
    puts ("thread2-2");
    sched_yield();
    randomSleep();
    pthread_mutex_unlock(&mutex2);
    randomSleep();
    puts ("thread2-3");
    randomSleep();
    pthread_mutex_lock(&mutex1);
    randomSleep();
    puts ("thread2-4");
    randomSleep();
    pthread_mutex_unlock(&mutex1);
    randomSleep();
}
