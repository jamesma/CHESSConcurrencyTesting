#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include <map>

#define THREAD_RUNNING_NOT_WAITING_FOR_LOCK     111
#define THREAD_RUNNING_WAITING_FOR_LOCK         222
#define THREAD_TERMINATED                       333

#define CURRENT_MODE                            0
#define DEBUG_MODE                              1

using namespace std;

struct Thread_Arg {
    // start_routine is ptr to a func taking one arg, void *, and returns void *
    void* (*start_routine)(void*);
    void* arg;
};

// Pointers to functions
int (*original_pthread_create)(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*) = NULL;
int (*original_pthread_join)(pthread_t, void**) = NULL;
int (*original_pthread_mutex_lock)(pthread_mutex_t*) = NULL;
int (*original_pthread_mutex_unlock)(pthread_mutex_t*) = NULL;

static void initialize_original_functions();

static pthread_t                                        CURRENT_THREAD = 0;
static map<pthread_mutex_t*, pthread_t>                 MUTEX_MAP;
static map<pthread_t, int>                              THREAD_MAP;
static pthread_mutex_t                                  GLOBAL_LOCK = PTHREAD_MUTEX_INITIALIZER;

static
void* thread_main(void *arg)
{
    struct Thread_Arg thread_arg = *(struct Thread_Arg*)arg;
    free(arg);

    pthread_t current = pthread_self();

    THREAD_MAP[current] = THREAD_RUNNING_NOT_WAITING_FOR_LOCK;

    // Enter a thread
    original_pthread_mutex_lock(&GLOBAL_LOCK);

    if (CURRENT_MODE == DEBUG_MODE)
        fprintf(stderr, "begin another thread: %u\n", current);

    void* ret = thread_arg.start_routine(thread_arg.arg);

    // Exit a thread
    original_pthread_mutex_unlock(&GLOBAL_LOCK);

    THREAD_MAP[current] = THREAD_TERMINATED;

    return ret;
}

extern "C"
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void*), void *arg)
{
    initialize_original_functions();

    struct Thread_Arg *thread_arg = (struct Thread_Arg*)malloc(sizeof(struct Thread_Arg));
    thread_arg->start_routine = start_routine;
    thread_arg->arg = arg;

    // Lock global lock before entering a new thread
    static bool mainThreadLocked = false;
    if (*thread == 0 && mainThreadLocked == false) {
        original_pthread_mutex_lock(&GLOBAL_LOCK);
        mainThreadLocked = true;
    }

    int ret = original_pthread_create(thread, attr, thread_main, thread_arg);

    return ret;
}

extern "C"
int pthread_join(pthread_t joinee, void **retval)
{
    initialize_original_functions();

    if (CURRENT_MODE == DEBUG_MODE)
        fprintf(stderr, "calling pthread_join on joinee thread: %u\n", joinee);

    original_pthread_mutex_unlock(&GLOBAL_LOCK);

    // Select joinee thread if joinee is still running
    if (THREAD_MAP[joinee] != THREAD_TERMINATED) {
        CURRENT_THREAD = joinee;
        if (CURRENT_MODE == DEBUG_MODE)
            fprintf(stderr, "\t\t\tpthread_join - executing joinee thread: %u\n", joinee);
    }
    
    // Wait until joinee thread terminates
    while (THREAD_MAP[joinee] != THREAD_TERMINATED) {}

    original_pthread_mutex_lock(&GLOBAL_LOCK);

    return original_pthread_join(joinee, retval);
}

extern "C"
int pthread_mutex_lock(pthread_mutex_t *mutex)
{
    initialize_original_functions();

    if (MUTEX_MAP[mutex] != 0) {
        original_pthread_mutex_unlock(&GLOBAL_LOCK);

        // Select next available thread to execute, we select the thread holding this lock
        CURRENT_THREAD = MUTEX_MAP[mutex];

        // Wait until program lock is not held by any other threads
        while (MUTEX_MAP[mutex] != 0) {}

        original_pthread_mutex_lock(&GLOBAL_LOCK);

        if (CURRENT_MODE == DEBUG_MODE)
            fprintf(stderr, "program lock1 on: %u\n", pthread_self());
        THREAD_MAP[pthread_self()] = THREAD_RUNNING_WAITING_FOR_LOCK;
        MUTEX_MAP[mutex] = pthread_self();
        //while (CURRENT_THREAD != pthread_self()) {fprintf(stderr, "current thread: %u\n", CURRENT_THREAD);}
    } else {
        if (CURRENT_MODE == DEBUG_MODE)
            fprintf(stderr, "program lock2 on: %u\n", pthread_self());
        THREAD_MAP[pthread_self()] = THREAD_RUNNING_WAITING_FOR_LOCK;
        MUTEX_MAP[mutex] = pthread_self();
        //while (CURRENT_THREAD != pthread_self()) {fprintf(stderr, "current thread: %u\n", CURRENT_THREAD);}
    }

    return original_pthread_mutex_lock(mutex);
}

extern "C"
int pthread_mutex_unlock(pthread_mutex_t *mutex)
{
    initialize_original_functions();

    if (CURRENT_MODE == DEBUG_MODE)
        if (MUTEX_MAP[mutex] != 0)
            fprintf(stderr, "\t\t\tpthread_mutex_unlock on %x with pthread_t %u\n", &mutex, MUTEX_MAP[mutex]);

    MUTEX_MAP[mutex] = 0;
    THREAD_MAP[pthread_self()] = THREAD_RUNNING_NOT_WAITING_FOR_LOCK;

    return original_pthread_mutex_unlock(mutex);
}

extern "C"
int sched_yield(void)
{
    original_pthread_mutex_unlock(&GLOBAL_LOCK);

    map<pthread_t, int>::iterator it;

    bool foundThread = false;
    for (it = THREAD_MAP.begin(); it != THREAD_MAP.end(); it++) {
        if ( (*it).first != pthread_self() && (*it).second == THREAD_RUNNING_NOT_WAITING_FOR_LOCK ) {
            if (CURRENT_MODE == DEBUG_MODE)
                fprintf(stderr, "\t\t\tsched_yield - found thread: %u with status: %d\n", (*it).first, (*it).second);
            CURRENT_THREAD = (*it).first;
            foundThread = true;
        }
    }
    if (!foundThread)
        CURRENT_THREAD = pthread_self();

    original_pthread_mutex_lock(&GLOBAL_LOCK);

    return 0;
}

static
void initialize_original_functions()
{
    static bool initialized = false;
    if (!initialized) {
        initialized = true;

        original_pthread_create =
            (int (*)(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*))dlsym(RTLD_NEXT, "pthread_create");
        original_pthread_join = 
            (int (*)(pthread_t, void**))dlsym(RTLD_NEXT, "pthread_join");
        original_pthread_mutex_lock =
            (int (*)(pthread_mutex_t*))dlsym(RTLD_NEXT, "pthread_mutex_lock");
        original_pthread_mutex_unlock =
            (int (*)(pthread_mutex_t*))dlsym(RTLD_NEXT, "pthread_mutex_unlock");
    }
}
